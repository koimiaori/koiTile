using System;
using System.IO;
using Microsoft.UI.Xaml.Media.Imaging;

namespace koiTile.Models
{
    internal sealed class PeekItem
    {
        public PeekItem(string path)
        {
            Path = path;
            try
            {
                Image = new BitmapImage
                {
                    CreateOptions = BitmapCreateOptions.IgnoreImageCache,
                    UriSource = new Uri(path),
                    DecodePixelWidth = 48
                };
            }
            catch
            {
                Image = null;
            }
        }

        public string Path { get; }

        public BitmapImage Image { get; }

        public string Display
        {
            get { return string.IsNullOrEmpty(Path) ? "" : System.IO.Path.GetFileName(Path); }
        }
    }
}
