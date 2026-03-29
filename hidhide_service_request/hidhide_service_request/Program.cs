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

        string utilitiesDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\', '/');
        string? parentDir = Directory.GetParent(utilitiesDir)?.FullName;
        string dirFullName = parentDir != null ? Path.Combine(parentDir, "DSX.exe") : "";
        if (dirFullName == "" || !File.Exists(dirFullName)) {
            NativeMethods.MsgBox(0, $"Couldn't find DSX.exe!\nSearched: {dirFullName}", "Error", 0);
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

        // Do not Disable/Enable the device: that causes Windows disconnect sounds, breaks open HID handles,
        // and can make DSX reconnect in a loop. HidHide's blocked list + active filter is sufficient.
        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine("HidHide configuration updated.");
        Console.ForegroundColor = ConsoleColor.White;
    }
}
