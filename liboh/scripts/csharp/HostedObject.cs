using System;
using System.Runtime.CompilerServices;
namespace Sirikata.Runtime {


public delegate void TimeoutCallback();

/** HostedObject bridges the C++/.NET gap. The public interface should be
 *  stable, but is a relatively thin wrapper around the internal interface.  The
 *  internal methods which actually cross the barrier *MUST* be marked as
 *  internal so the C++ implementation can change as needed.
 */
public class HostedObject {

    // ID Related Functions

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern void iInternalID(out Guid result);

    /** Get this object's internal UUID. */
    public static Guid InternalID() {
        Guid result = new Guid();
        iInternalID(out result);
        return result;
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern void iObjectReference(ref Guid space, out Guid result);

    /** Get this object's reference within the given space.  If the object isn't
     *  connected to the space, returns a null Guid.
     */
    public static Guid ObjectReference(Guid space) {
        Guid result = new Guid();
        iObjectReference(ref space, out result);
        return result;
    }



    // Time/Timer Related Functions

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern void iLocalTime(out Sirikata.Runtime.Time retval);

    /** Get the current local time, i.e. the current time with epoch equal to
     *  the starting time of this object host.
     */
    public static Sirikata.Runtime.Time LocalTime() {
        Sirikata.Runtime.Time retval = new Sirikata.Runtime.Time();
        iLocalTime(out retval);
        return retval;
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern void iTime(ref Guid spaceid, out Sirikata.Runtime.Time retval);

    /** Get the current time in the specified space, i.e. the current time with
     *  epoch equal to the starting time of the object host.  This is only an
     *  approximation and may not be monotonic due to resynchronization.
     */
    public static Sirikata.Runtime.Time Time(Guid spaceid) {
        Sirikata.Runtime.Time retval = new Sirikata.Runtime.Time();
        iTime(ref spaceid, out retval);
        return retval;
    }


    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern void iTickPeriod(Sirikata.Runtime.TimeClass t);

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern void iAsyncWait(TimeoutCallback callback, Sirikata.Runtime.TimeClass t);

    internal static long TimeTicks(Sirikata.Runtime.Time t) {
        return t.microseconds();
    }
    public static  void SetupTickFunction(TimeoutCallback tc, Sirikata.Runtime.Time t){

        long us=t.microseconds();
        TimeoutCallback wrapped_tc=null;
        long estLocalTime=LocalTime().microseconds()+us*2;
        wrapped_tc=new TimeoutCallback(delegate(){
                try {
                    tc();
                }finally {
                    long delta=estLocalTime-LocalTime().microseconds();
                    estLocalTime+=us;
                    if (delta>0) {
                        iAsyncWait(wrapped_tc,new TimeClass((ulong)delta));
                    }else {
                        iAsyncWait(wrapped_tc,new TimeClass());
                    }
                }
            });
        iAsyncWait(wrapped_tc,t.toClass());
    }
    public static bool AsyncWait(TimeoutCallback callback, Sirikata.Runtime.Time t) {
        if (callback==null) return false;
        iAsyncWait(callback,t.toClass());
        return true;
    }


    // Messaging Related Functions

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    internal static extern bool iSendMessage(ref Guid dest_space, ref Guid dest_obj, uint dest_port, byte[] payload);

    /** Send a message from this object.
     *  \param dest_space destination space
     *  \param dest_obj destination object
     *  \param dest_port destination ODP port
     *  \param payload the contents of the message
     *  \returns true if message was accepted, false if there wasn't queue space available
     */
    public static bool SendMessage(Guid dest_space, Guid dest_obj, uint dest_port, byte[] payload) {
        if (payload == null || payload.Length == 0)
            return false;
        return iSendMessage(ref dest_space, ref dest_obj, dest_port, payload);
    }

}
}