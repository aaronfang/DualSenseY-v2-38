using NAudio.CoreAudioApi;
using NAudio.Wave;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;

namespace AudioPassthrough
{
    public partial class Form1 : Form
    {
        private MMDevice device = null;
        private MMDevice mmdeviceplayback;
        private WasapiOut hapticStream;
        private WasapiOut audioPassthroughStream;
        private WasapiOut hapticsToSpeakerStream;
        private WasapiLoopbackCapture wasapiLoopbackCapture = null;
        private MMDeviceEnumerator mmdeviceEnumerator = new MMDeviceEnumerator();
        private bool StartNewPlayback = true;
        private BufferedWaveProvider audioPassthroughBuffer = null;
        private float speakerPlaybackVolume = 0;
        private float leftActuatorVolume = 1;
        private float rightActuatorVolume = 1;
        private bool SystemAudioPlayback = true;
        private string audioID = string.Empty;
        private string arg = string.Empty;
        private MMDevice currentPlaybackDevice;

        public Form1(string[] args)
        {
            InitializeComponent();

            if (args.Length == 0)
            {
                MessageBox.Show("No arguments!");
                Environment.Exit(0);
            }
            arg = args[0];

            Process[] processes = Process.GetProcessesByName("AudioPassthrough");
            foreach (Process process in processes)
            {
                string windowTitle = WindowHelper.GetWindowTitle(process.Id);
                if (windowTitle.Contains(arg))
                {
                    Environment.Exit(0); // Close the app if audio to haptics is already running for this controller
                }
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            this.Focus();
            new Thread(LookForNewDevice).Start();
            StartAudioToHaptics(arg.ToLower());
        }

        private void LookForNewDevice()
        {
            MMDeviceEnumerator enumerator = new MMDeviceEnumerator();
            currentPlaybackDevice = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            Thread.Sleep(5000);
            bool disconnected = false;

            while (true)
            {
                MMDevice newDevice = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);

                device = null;

                foreach (MMDevice mmdevice in mmdeviceEnumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active))
                {
                    try
                    {
                        if (mmdevice.FriendlyName.ToLower() == arg.ToLower())
                        {
                            device = mmdevice;
                            break;
                        }
                    }
                    catch
                    {
                        continue;
                    }
                }

                if (device == null)
                {
                    disconnected = true;
                }
                else if (device != null && disconnected)
                {
                    StartAudioToHaptics(arg.ToLower(), true);
                    disconnected = false;
                }

                if (newDevice.State == DeviceState.Active && newDevice.FriendlyName != currentPlaybackDevice.FriendlyName && !newDevice.FriendlyName.Contains("Wireless Controller")) {
                    currentPlaybackDevice = newDevice;
                    StartAudioToHaptics(arg.ToLower());
                }

                Thread.Sleep(3000);
            }
        }

        private void StartAudioToHaptics(string instanceID = "", bool reconnectattempt = false)
        {
            device = null;
            foreach (MMDevice mmdevice in mmdeviceEnumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active))
            {
                try
                {
                    if (mmdevice.FriendlyName.ToLower() == arg.ToLower())
                    {
                        device = mmdevice;
                        audioID = instanceID;
                        break;
                    }
                }
                catch
                {
                    continue;
                }
            }

            if (reconnectattempt == false) // Return if both failed
            {
                if (device == null || device.State == DeviceState.NotPresent || device.State == DeviceState.Unplugged || device.State == DeviceState.Disabled)
                {
                    MessageBox.Show("Couldn't find DualSense Wireless Controller Speaker!");
                    Environment.Exit(0);
                }
            }
            else
            {
                if (device == null || device.State == DeviceState.NotPresent || device.State == DeviceState.Unplugged || device.State == DeviceState.Disabled)
                {
                    return;
                }
            }

            this.Text = device.FriendlyName;

            // AUDIO PASSTHROUGH STREAM
            setNewPlayback();
            audioPassthroughStream = new WasapiOut(device, AudioClientShareMode.Shared, true, 1);
            audioPassthroughBuffer = new BufferedWaveProvider(wasapiLoopbackCapture.WaveFormat)
            {
                BufferLength = 100000,
                ReadFully = true,
                DiscardOnBufferOverflow = true
            };

            MultiplexingWaveProvider multiplexingWaveProviderAP = new MultiplexingWaveProvider(new BufferedWaveProvider[] { audioPassthroughBuffer }, 4);
            multiplexingWaveProviderAP.ConnectInputToOutput(0, 0);
            multiplexingWaveProviderAP.ConnectInputToOutput(1, 1);
            multiplexingWaveProviderAP.ConnectInputToOutput(0, 2);
            multiplexingWaveProviderAP.ConnectInputToOutput(1, 3);
            audioPassthroughStream.Init(multiplexingWaveProviderAP);
            audioPassthroughStream.Play();

            trackBar1.Invoke((MethodInvoker)delegate
            {
                if (trackBar1.Value != 0)
                {
                    setVolume((float)(Convert.ToDouble(trackBar1.Value) / 100), 1, 1);
                }
                else
                {
                    setVolume(0, 1, 1);
                }
            });
        }

        private void setNewPlayback()
        {
            if (wasapiLoopbackCapture != null)
            {
                wasapiLoopbackCapture.StopRecording();
                wasapiLoopbackCapture.Dispose();
            }

            mmdeviceplayback = mmdeviceEnumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            wasapiLoopbackCapture = new WasapiLoopbackCapture(mmdeviceplayback);
            wasapiLoopbackCapture.DataAvailable += WasapiLoopbackCapture_DataAvailable;
            wasapiLoopbackCapture.StartRecording();
        }

        private void WasapiLoopbackCapture_DataAvailable(object? sender, WaveInEventArgs e)
        {
            if (SystemAudioPlayback)
            {
                float gainFactor = trackBar2.Value;
                if (gainFactor != 1.0f) // Only process if gain adjustment is needed
                {
                    Span<byte> buffer = e.Buffer.AsSpan(0, e.BytesRecorded);
                    Span<float> samples = MemoryMarshal.Cast<byte, float>(buffer);
                    for (int i = 0; i < samples.Length; i++)
                    {
                        samples[i] = Math.Clamp(samples[i] * gainFactor, -1.0f, 1.0f);
                    }
                }
                audioPassthroughBuffer.AddSamples(e.Buffer, 0, e.BytesRecorded);
            }
        }

        private void setVolume(float speaker, float left, float right)
        {
            speakerPlaybackVolume = speaker;
            leftActuatorVolume = left;
            rightActuatorVolume = right;

            try
            {
                if (audioPassthroughStream != null)
                {
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(0, speakerPlaybackVolume);
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(1, speakerPlaybackVolume);
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(2, leftActuatorVolume);
                    audioPassthroughStream.AudioStreamVolume.SetChannelVolume(3, rightActuatorVolume);
                }
            }
            catch (ArgumentOutOfRangeException)
            {
                throw new ArgumentOutOfRangeException("Volume must be between 0.0 and 1.0.");
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (hapticStream != null)
            {
                hapticStream.Dispose();
            }

            if (audioPassthroughStream != null)
            {
                audioPassthroughStream.Dispose();
                audioPassthroughBuffer.ClearBuffer();
            }

            if (wasapiLoopbackCapture != null)
            {
                wasapiLoopbackCapture.Dispose();
            }

            Environment.Exit(0);
        }

        private void trackBar1_Scroll(object sender, EventArgs e)
        {
            if (trackBar1.Value == 0)
            {
                setVolume(0, 1, 1);
            }
            else
            {
                setVolume((float)(Convert.ToDouble(trackBar1.Value) / 100), 1, 1);
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            this.WindowState = FormWindowState.Minimized;
        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            if (hapticStream != null)
            {
                hapticStream.Dispose();
            }

            if (audioPassthroughStream != null)
            {
                audioPassthroughStream.Dispose();
                audioPassthroughBuffer.ClearBuffer();
            }

            if (wasapiLoopbackCapture != null)
            {
                wasapiLoopbackCapture.Dispose();
            }

            Environment.Exit(0);
        }
    }

    internal class WindowHelper
    {
        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr FindWindowEx(IntPtr parentHandle, IntPtr childAfter, string className, string windowTitle);

        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        public static extern int GetWindowText(IntPtr hwnd, StringBuilder lpString, int nMaxCount);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);

        public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

        public static string GetWindowTitle(int processId)
        {
            StringBuilder windowTitle = new StringBuilder(255);
            EnumWindows((hwnd, lParam) =>
            {
                int pid;
                GetWindowThreadProcessId(hwnd, out pid);
                if (pid == processId)
                {
                    GetWindowText(hwnd, windowTitle, windowTitle.Capacity);
                    return false; // Stop enumeration once we have the window title
                }
                return true;
            }, IntPtr.Zero);

            return windowTitle.ToString();
        }

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        public static extern int GetWindowThreadProcessId(IntPtr hwnd, out int processId);
    }
}
