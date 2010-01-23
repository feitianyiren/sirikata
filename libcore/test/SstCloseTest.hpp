#include "network/Stream.hpp"
#include "network/StreamListener.hpp"
#include "network/StreamFactory.hpp"
#include "network/StreamListenerFactory.hpp"
#include "network/IOServiceFactory.hpp"
#include "network/IOServicePool.hpp"
#include "network/IOService.hpp"
#include "util/AtomicTypes.hpp"
#include "util/PluginManager.hpp"
#include "util/DynamicLibrary.hpp"
#include "task/Time.hpp"
#include <cxxtest/TestSuite.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <time.h>

using namespace Sirikata::Network;
using namespace Sirikata;
class SstCloseTest : public CxxTest::TestSuite
{
    enum {
        NUM_TEST_STREAMS=3
    };
    IOServicePool* mSendService;
    IOServicePool* mRecvService;
    String mPort;
    int mBytes;
    int mChunks;
    StreamListener * mListener;
    ///only close streams higher than moffset
    int mOffset;
    int mSendReceiveMap[NUM_TEST_STREAMS];
    Stream * mSenders[NUM_TEST_STREAMS];
    Stream * mReceivers[NUM_TEST_STREAMS];
    //number of bytes received *2 + 1 if closed
    AtomicValue<int> mReceiversData[NUM_TEST_STREAMS];
    void listenerConnectionCallback(int id, Stream::ConnectionStatus stat, const std::string&reason) {
        if (stat!=Stream::Connected) {
            if(mSendReceiveMap[id]==-1) {
                TS_ASSERT(mSendReceiveMap[id]!=-1);
            }else{
                mReceiversData[mSendReceiveMap[id]]+=1;
            }
            //printf("%d closed=%d\n",id,mReceiversData[id].read());
/*
            void *bt[16384];
            int size=backtrace(bt,16384);
            backtrace_symbols_fd(bt,size,0);
*/
        }
    }
    void connectionCallback(int id, Stream::ConnectionStatus stat, const std::string&reason) {
        if (stat!=Stream::Connected) {
            TS_ASSERT(false);
            printf ("CONNECTION FAIL\n");
        }else {
            //printf ("ACTIVE\n");
        }
    }
    Network::Stream::ReceivedResponse listenerDataRecvCallback(Stream *s,int id, const Chunk&data) {
        if (mSendReceiveMap[id]==-1) {
            mSendReceiveMap[id]=(unsigned char)data[0];
        }
        static bool pause=true;
        if (rand()<RAND_MAX/5||pause)
            pause=!pause;
        if (pause) {
            mRecvService->service()->post(std::tr1::bind(&Stream::readyRead,s));
            return Network::Stream::PauseReceive;
        }
        mReceiversData[mSendReceiveMap[id]]+=(int)(data.size()*2);
        return Network::Stream::AcceptedData;
    }

    void listenerNewStreamCallback (Stream * newStream, Stream::SetCallbacks& setCallbacks) {
        if (newStream) {
            for (int i=0;i<NUM_TEST_STREAMS;++i) {
                if (!mReceivers[i]) {
                    mReceivers[i]=newStream;
                    using std::tr1::placeholders::_1;
                    using std::tr1::placeholders::_2;
                    setCallbacks(std::tr1::bind(&SstCloseTest::listenerConnectionCallback,this,i,_1,_2),
                                 std::tr1::bind(&SstCloseTest::listenerDataRecvCallback,this,newStream,i,_1),
                                 &Stream::ignoreReadySendCallback);
                    break;
                }
            }
        }
    }
public:
    static SstCloseTest*createSuite() {
        return new SstCloseTest;
    }
    static void destroySuite(SstCloseTest*sst) {
        delete sst;
    }
private:
    SstCloseTest() {
        Sirikata::PluginManager plugins;
        plugins.load( Sirikata::DynamicLibrary::filename("tcpsst") );

        uint32 randport = 3000 + (uint32)(Sirikata::Task::LocalTime::now().raw() % 20000);
        mPort = boost::lexical_cast<std::string>(randport);

        mBytes=65536;
        mChunks=3;
        mOffset=1;
        mSendService = new IOServicePool(1);
        mRecvService = new IOServicePool(1);
        mListener = StreamListenerFactory::getSingleton().getDefaultConstructor()(mRecvService->service(),StreamListenerFactory::getSingleton().getDefaultOptionParser()(String()));
        using std::tr1::placeholders::_1;
        using std::tr1::placeholders::_2;
        mListener->listen(Address("127.0.0.1",mPort),std::tr1::bind(&SstCloseTest::listenerNewStreamCallback,this,_1,_2));

        mRecvService->run();
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif

    }
public:
    void closeStreamRun(bool fork, bool doSleep=false) {
        using std::tr1::placeholders::_1;
        using std::tr1::placeholders::_2;

        for(int i=0;i<NUM_TEST_STREAMS;++i) {
            mReceivers[i]=mSenders[i]=NULL;
            mSendReceiveMap[i]=-1;
            mReceiversData[i]=0;
        }

        for(int i=0;i<NUM_TEST_STREAMS;++i) {
            if (i==0||!fork) {
                mSenders[i]=StreamFactory::getSingleton().getDefaultConstructor()(mSendService->service(),StreamFactory::getSingleton().getDefaultOptionParser()(String("--parallel-sockets=1")));
                mSenders[i]->connect(Address("127.0.0.1",mPort),
                                     &Stream::ignoreSubstreamCallback,
                                     std::tr1::bind(&SstCloseTest::connectionCallback,this,i,_1,_2),
                                     &Stream::ignoreReceivedCallback,
                                     &Stream::ignoreReadySendCallback);
            }else {
                mSenders[i]=mSenders[0]->clone(  std::tr1::bind(&SstCloseTest::connectionCallback,this,i,_1,_2),
                                     &Stream::ignoreReceivedCallback,
                                     &Stream::ignoreReadySendCallback);

            }

        }
        mSendService->reset();
        mSendService->run();
#ifdef _WIN32
        //Sleep(1000);
#else
//        sleep(1);
#endif

        int sentSoFar=0;
        for (int c=0;c<mChunks;++c) {
            int willHaveSent=(c+1)*mBytes/mChunks;

            Chunk cur(willHaveSent-sentSoFar);
            for (size_t j=0;j<cur.size();++j) {
                cur[j]=j+sentSoFar;
            }
            for (int i=0;i<NUM_TEST_STREAMS;++i) {
                if (cur.size()) {
                    cur[0]=i%256;
                    if (cur.size()>1) {
                        cur[1]=i/256%256;
                    }
                    if (cur.size()>2) {
                        cur[2]=i/256/256%256;
                    }
                    if (cur.size()>3) {
                        cur[3]=i/256/256/256%256;
                    }
                }
                bool retval=mSenders[i]->send(MemoryReference(cur),ReliableUnordered);
                TS_ASSERT(retval);

            }
            if (doSleep) {
#ifdef _WIN32
                Sleep(1000);
#else
                sleep(1);
#endif
            }
            sentSoFar=willHaveSent;
        }
        if (!doSleep) {
#ifdef _WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
        }
        for (int i=mOffset;i<NUM_TEST_STREAMS;++i) {
            mSenders[i]->close();
        }
        bool done=true;
        int counter=0;
        do {
            done=true;
            for (int i=mOffset;i<NUM_TEST_STREAMS;++i) {
                if (mReceiversData[i].read()!=mBytes*2+1) {
                    done=false;
                    if (counter>3) {
                        SILOG(ssttest,error,"Data "<<i<<" only "<<mReceiversData[i].read()<<" != "<<mBytes*2+1);
                    }
                }
            }
            if (!done) {
#ifdef _WIN32
                Sleep(1000);
#else
                sleep (1);
#endif
            }
            ++counter;
        }while (counter<5000&&!done);
        TS_ASSERT(done);
        for (int i=0;i<NUM_TEST_STREAMS;++i) {
            delete mSenders[i];
            mSenders[i]=NULL;
        }
        for (int i=0;i<NUM_TEST_STREAMS;++i) {
            delete mReceivers[i];
            mReceivers[i]=NULL;
        }
    }
    void testCloseAllStream() {
        mOffset=0;
        closeStreamRun(false);
    }
    void testCloseAllStreams() {
        mOffset=0;
        closeStreamRun(true);
    }
    void testCloseSomeStream() {
        mOffset=1;
        closeStreamRun(false);
    }
    void testCloseSomeStreams() {
        mOffset=1;
        closeStreamRun(true);
    }
    void xestCloseSomeStreamSleep() {
        mOffset=1;
        closeStreamRun(false,true);
    }
    void xestCloseSomeStreamsSleep() {
        mOffset=1;
        closeStreamRun(true,true);
    }
    ~SstCloseTest() {

        delete mListener;
        mSendService->join();
        mRecvService->join();
        delete mSendService;
        delete mRecvService;

    }
};
