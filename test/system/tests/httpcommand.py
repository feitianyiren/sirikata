#!/usr/bin/python

from __future__ import print_function
from framework.tests.http import HttpCommandTest
import os.path, subprocess, traceback
from framework.procset import ProcSet

class SingleHttpServerTest(HttpCommandTest):
    '''Runs a single service (space, cppoh) and checks that it gets shutdown cleanly'''

    duration = 30
    wait_until_responsive = True

    # You must override these settings
    service_name = None # e.g., 'space'
    extra_args = {} # Additional command line args for the service

    def runTest(self):
        service_output_filename = os.path.join(self._folder, self.service_name + '.log')

        self.add_http(self.service_name)
        service_cmd = [ self._binaries[self.service_name] ]
        service_cmd += self.http_settings(self.service_name)
        service_cmd += ['--' + k + '=' + v for k,v in self.extra_args.iteritems()]

        procs = ProcSet()
        service_output = open(service_output_filename, 'w')
        print(' '.join(service_cmd), file=service_output)
        service_output.flush()
        procs.process(self.service_name, service_cmd, stdout=service_output, stderr=subprocess.STDOUT, default=True)

        # If requested, block for upto 10 seconds for the process to start responding to commands
        if self.wait_until_responsive:
            result = self.command(self.service_name, 'meta.commands', retries=40, wait=.25)
            if result is None: self.fail('Server never became responsive to HTTP commands')

        # Defer to the implementation for sending and checking test
        # commands. Catch all errors so we can make sure we cleanup the
        # processes properly and report crashes, etc.
        try:
            self.testCommands(procs)
        except:
            self.fail(msg='Uncaught exception during test:\n' + traceback.format_exc())

        # The implementation should know whether things are shutting
        # down cleanly. We just make sure we clean up aggressively if
        # necessary.
        if not procs.done():
            procs.wait(until=self.duration, killAt=self.duration, output=self._output)

        # Print a notification if we had to kill this process
        if procs.killed():
            self.fail('Timed out')

        service_output.close()
        self.report_files = {self.service_name: service_output_filename}

        self.assertLogErrorFree(service_output_filename)
        self.assertReturnCode(procs.returncode())

    def report(self):
        print("Execution Log:", file=self._output)
        for report_name, report_file in self.report_files.iteritems():
            print("  ", report_name, file=self._output)
            fp = open(report_file, 'r')
            for line in fp.readlines():
                print("    ", line, end='', file=self._output)
            fp.close()
            print(file=self._output)


class SpaceHttpCommandTest(SingleHttpServerTest):
    '''Runs a space server and checks that it gets shutdown cleanly'''
    service_name = 'space'


class OHHttpCommandTest(SingleHttpServerTest):
    '''Runs an object host and checks that it gets shutdown cleanly'''
    service_name = 'cppoh'



class SpaceMetaCommandsTest(SpaceHttpCommandTest):
    '''
    Sanity check that the command processor was instantiated and works
    for the simplest command that is always guaranteed to work
    '''
    duration = 10

    # The one test where we don't want to block until a command gets through
    wait_until_responsive = False

    def testCommands(self, procs):
        # meta.commands is always available, even if empty
        result = self.command(self.service_name, 'meta.commands', retries=40, wait=.25)
        self.assertIsNotNone(result)
        # We need to hup this one because we don't try to shutdown cleanly
        procs.hup(output=self.output)

class SpaceShutdownTest(SpaceHttpCommandTest):
    '''
    Check that the space server can be shutdown by external command
    '''

    after = [SpaceMetaCommandsTest]

    def testCommands(self, procs):
        self.assertIsNotNone( self.command(self.service_name, 'context.shutdown') )


class OHObjectTest(OHHttpCommandTest):
    '''
    Check object creation, listing, and destruction.
    '''
    after = [SpaceShutdownTest]
    extra_args = {
        'object-factory-opts': '--db=not-a-real-db.db' # Disable loading objects from db
        }

    def testCommands(self, procs):
        # Should start empty
        response = self.command(self.service_name, 'oh.objects.list')
        self.assertIsNotNone(response)
        self.assertIsIn('objects', response)
        self.assertEmpty(response['objects'])

        # Invalid object creation requests
        #  No body
        response = self.command(self.service_name, 'oh.objects.create')
        self.assertIsNotNone(response)
        self.assertIsIn('error', response)
        self.assertIsNotIn('id', response)

        #  Body missing script type
        response = self.command(self.service_name, 'oh.objects.create', { 'script' : { 'contents': 'system.print("hi")'} })
        self.assertIsNotNone(response)
        self.assertIsIn('error', response)
        self.assertIsNotIn('id', response)

        # Valid object creation request -- assumes js plugin
        response = self.command(self.service_name, 'oh.objects.create',
                                { 'script' :
                                  { 'type' : 'js',
                                    'contents': 'system.print("hi")'
                                    }
                                  })
        self.assertIsNotNone(response)
        self.assertIsNotIn('error', response)
        self.assertIsIn('id', response)
        obj1_id = response['id']
        self.assertNotEmpty(obj1_id)

        # Check against IDs from listing
        response = self.command(self.service_name, 'oh.objects.list')
        self.assertIsNotNone(response)
        self.assertIsIn('objects', response)
        self.assertNotEmpty(response['objects'])
        self.assertIsIn(obj1_id, response['objects'])

        # Another object. We need at least two to keep things alive
        response = self.command(self.service_name, 'oh.objects.create',
                                { 'script' :
                                  { 'type' : 'js',
                                    'contents': 'system.print("hi")'
                                    }
                                  })
        self.assertIsNotNone(response)
        self.assertIsNotIn('error', response)
        self.assertIsIn('id', response)
        obj2_id = response['id']
        self.assertNotEmpty(obj2_id)

        # Check against IDs from listing
        response = self.command(self.service_name, 'oh.objects.list')
        self.assertIsNotNone(response)
        self.assertIsIn('objects', response)
        self.assertLen(response['objects'], 2)
        self.assertIsIn(obj1_id, response['objects'])
        self.assertIsIn(obj2_id, response['objects'])

        # Deletions.
        #  Invalid request: no body
        response = self.command(self.service_name, 'oh.objects.destroy')
        self.assertIsNotNone(response)
        self.assertIsIn('error', response)
        #  Invalid request: invalid ID
        response = self.command(self.service_name, 'oh.objects.destroy', { 'object' : 'invalid id string' })
        self.assertIsNotNone(response)
        self.assertIsIn('error', response)
        #  Invalid request: ID that doesn't exist
        response = self.command(self.service_name, 'oh.objects.destroy', { 'object' : 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa' })
        self.assertIsNotNone(response)
        self.assertIsIn('error', response)

        # Valid destruction request: remove obj1
        response = self.command(self.service_name, 'oh.objects.destroy', { 'object' : obj1_id })
        self.assertIsNotNone(response)
        self.assertIsIn('success', response)

        # Check removal from IDs listing
        response = self.command(self.service_name, 'oh.objects.list')
        self.assertIsNotNone(response)
        self.assertIsIn('objects', response)
        self.assertIsNotIn(obj1_id, response['objects'])
        self.assertIsIn(obj2_id, response['objects'])
        self.assertLen(response['objects'], 1)

        # Finally, destroy the last item. This should shut down the server
        response = self.command(self.service_name, 'oh.objects.destroy', { 'object' : obj2_id })
        self.assertIsNotNone(response)
        self.assertIsIn('success', response)
