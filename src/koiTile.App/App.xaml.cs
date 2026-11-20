using System;
using koiTile.Interop;
using koiTile.Services;
using Microsoft.UI.Xaml;

namespace koiTile
{
    public partial class App : Application
    {
        public static IntPtr WindowHandle { get; set; }

        private Window _window;

        public App()
        {
            InitializeComponent();
            UnhandledException += OnUnhandledException;
            AppDomain.CurrentDomain.ProcessExit += OnProcessExit;
        }

        protected override void OnLaunched(LaunchActivatedEventArgs args)
        {
            Core.kt_init();
            _window = new MainWindow();
            _window.Activate();
        }

        private void OnUnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
        {
            Log.Write("unhandled: " + e.Message);
            Log.Write(e.Exception == null ? "" : e.Exception.ToString());
        }

        private void OnProcessExit(object sender, EventArgs e)
        {
            try
            {
                Core.kt_shutdown();
            }
            catch
            {
            }
        }
    }
}
