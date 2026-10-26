using System;
using System.Runtime.InteropServices;

// Read-only ETW session query. No providers or logger state are changed.
public static class D2vrTraceStats {
    [StructLayout(LayoutKind.Sequential)]
    struct Wnode {
        public uint BufferSize, ProviderId;
        public ulong HistoricalContext;
        public long TimeStamp;
        public Guid Guid;
        public uint ClientContext, Flags;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct Properties {
        public Wnode Wnode;
        public uint BufferSize, MinimumBuffers, MaximumBuffers, MaximumFileSize;
        public uint LogFileMode, FlushTimer, EnableFlags;
        public int AgeLimit;
        public uint NumberOfBuffers, FreeBuffers, EventsLost, BuffersWritten;
        public uint LogBuffersLost, RealTimeBuffersLost;
        public IntPtr LoggerThreadId;
        public uint LogFileNameOffset, LoggerNameOffset;
    }
    public sealed class Result {
        public uint Error, EventsLost, LogBuffersLost, RealTimeBuffersLost;
    }
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, EntryPoint="ControlTraceW")]
    static extern uint ControlTrace(ulong handle, string name, IntPtr properties, uint control);
    public static Result Query(string name) {
        int size=Marshal.SizeOf(typeof(Properties));
        IntPtr memory=Marshal.AllocHGlobal(size+4096);
        try {
            Marshal.Copy(new byte[size+4096],0,memory,size+4096);
            var p=new Properties();
            p.Wnode.BufferSize=(uint)(size+4096);
            p.LoggerNameOffset=(uint)size;
            p.LogFileNameOffset=(uint)(size+2048);
            Marshal.StructureToPtr(p,memory,false);
            uint error=ControlTrace(0,name,memory,0);
            p=(Properties)Marshal.PtrToStructure(memory,typeof(Properties));
            return new Result { Error=error, EventsLost=p.EventsLost,
                LogBuffersLost=p.LogBuffersLost, RealTimeBuffersLost=p.RealTimeBuffersLost };
        } finally { Marshal.FreeHGlobal(memory); }
    }
}
