using System;
using System.IO;

namespace koiTile.Services
{
    internal static class Log
    {
        private static readonly object Gate = new object();
        private static readonly string FilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "koiTile", "koiTile.log");

        public static void Write(string message)
        {
            try
            {
                lock (Gate)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
                    File.AppendAllText(FilePath, DateTime.Now.ToString("HH:mm:ss.fff") + " " + message + Environment.NewLine);
                }
            }
            catch
            {
            }
        }
    }
}
