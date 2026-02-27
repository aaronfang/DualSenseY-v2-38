using Microsoft.VisualBasic;
using Nefarius.Drivers.HidHide;
using Nefarius.Utilities.DeviceManagement.PnP;
using System.Runtime.InteropServices;

public static class NativeMethods {

    // Declares managed prototypes for unmanaged functions.
    [DllImport("User32.dll", EntryPoint = "MessageBox",
        CharSet = CharSet.Auto)]
    internal static extern int MsgBox(
        IntPtr hWnd, string lpText, string lpCaption, uint uType);

    // Causes incorrect output in the message window.
    [DllImport("User32.dll", EntryPoint = "MessageBoxW",
        CharSet = CharSet.Ansi)]
    internal static extern int MsgBox2(
        IntPtr hWnd, string lpText, string lpCaption, uint uType);

    // Causes an exception to be thrown. EntryPoint, CharSet, and
    // ExactSpelling fields are mismatched.
    [DllImport("User32.dll", EntryPoint = "MessageBox",
        CharSet = CharSet.Ansi, ExactSpelling = true)]
    internal static extern int MsgBox3(
        IntPtr hWnd, string lpText, string lpCaption, uint uType);
}

public class Program {

    private static void Main(string[] args) {
        if (args.Length != 2) {
            NativeMethods.MsgBox(0, "Not enough arguments!", "Error", 0);
            return;
        }

        HidHideControlService hidHide = new HidHideControlService();

        if (!hidHide.IsInstalled) {
            Environment.Exit(0);
            return;
        }

        string dirFullName = AppDomain.CurrentDomain.BaseDirectory.Replace("utilities\\", "DSX.exe");
        if (!File.Exists(dirFullName)) {
            NativeMethods.MsgBox(0, $"Couldn't find DSX.exe!\nError path: {dirFullName}", "Error", 0);
            return;
        }

        Console.WriteLine("Adding application path to HidHide...");
        hidHide.AddApplicationPath(dirFullName);

        string instanceID = PnPDevice.GetInstanceIdFromInterfaceId(args[0]);
        if (args[1] == "hide") {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"[+] Adding instance ID {instanceID} to blocked list...");
            hidHide.AddBlockedInstanceId(instanceID);
            hidHide.IsAppListInverted = false;
            hidHide.IsActive = true;
        }
        else if (args[1] == "show") {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"[-] Removing instance ID {instanceID} from blocked list...");
            hidHide.RemoveBlockedInstanceId(instanceID);
            hidHide.IsActive = false;
        }

        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.WriteLine("Searching device by instance ID...");
        Console.ForegroundColor = ConsoleColor.White;
        PnPDevice Device = PnPDevice.GetDeviceByInstanceId(instanceID);
        try {
            Console.WriteLine("Disabling the device... Please wait");
            Device.Disable();
        }
        catch { }
        Console.WriteLine("Re-enabling the device...");
        Device.Enable();
    }
}
