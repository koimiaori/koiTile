using System;
using System.Collections.Generic;
using System.IO;
using koiTile.Interop;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;

namespace koiTile.Services
{
    internal static class TileIcons
    {
        private static readonly Dictionary<string, ImageSource> Cache = new Dictionary<string, ImageSource>();

        public static double Size(int size)
        {
            if (size == 0)
            {
                return 24;
            }
            if (size == 3)
            {
                return 64;
            }
            return 32;
        }

        public static ImageSource Source(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
            {
                return null;
            }
            if (Cache.TryGetValue(path, out ImageSource cached))
            {
                return cached;
            }
            ImageSource source = null;
            try
            {
                string resolved = path;
                if (Path.GetExtension(resolved).ToLowerInvariant() == ".lnk")
                {
                    resolved = Core.ShortcutTarget(resolved);
                }
                if (File.Exists(resolved))
                {
                    string extension = Path.GetExtension(resolved).ToLowerInvariant();
                    if (extension == ".exe" || extension == ".dll" || extension == ".ico")
                    {
                        string target = Path.Combine(Workspace.Instance.TempDir, "board_icon_" + (uint)path.GetHashCode() + ".png");
                        if (Core.kt_image_extract_icon(Core.Utf8(resolved), 0, 128, Core.Utf8(target)) == 0)
                        {
                            resolved = target;
                        }
                        else
                        {
                            resolved = "";
                        }
                    }
                    if (resolved.Length > 0)
                    {
                        source = new BitmapImage
                        {
                            CreateOptions = BitmapCreateOptions.IgnoreImageCache,
                            UriSource = new Uri(resolved)
                        };
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("icon preview: " + ex.Message);
            }
            Cache[path] = source;
            return source;
        }
    }
}
