using System;
using System.Runtime.InteropServices;

namespace koiTile.Services
{
    internal static class AppActivator
    {
        [ComImport]
        [Guid("2E941141-7F97-4756-BA1D-9DECDE894A3D")]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        private interface IApplicationActivationManager
        {
            int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)] string appUserModelId, [MarshalAs(UnmanagedType.LPWStr)] string arguments, int options, out uint processId);
        }

        [ComImport]
        [Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")]
        private class ApplicationActivationManager
        {
        }

        public static bool Activate(string aumid, string arguments)
        {
            if (string.IsNullOrWhiteSpace(aumid))
            {
                return false;
            }
            try
            {
                var manager = (IApplicationActivationManager)new ApplicationActivationManager();
                int result = manager.ActivateApplication(aumid, arguments ?? "", 0, out uint processId);
                if (result != 0)
                {
                    Log.Write("activate failed " + result.ToString("X8"));
                    return false;
                }
                return true;
            }
            catch (Exception ex)
            {
                Log.Write("activate: " + ex.Message);
                return false;
            }
        }
    }
}
