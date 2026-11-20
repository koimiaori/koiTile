using System;
using System.IO;
using koiTile.Interop;
using koiTile.Models;
using koiTile.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Storage.Pickers;

namespace koiTile.Pages
{
    public sealed partial class IconPage : Page, ITilePage
    {
        private static readonly string[] IconExtensions = { ".exe", ".lnk", ".ico", ".jpeg", ".jpg", ".png", ".webp", ".dll", ".bmp" };

        public IconPage()
        {
            InitializeComponent();
            Loaded += (s, e) => LoadTile();
        }

        public void LoadTile()
        {
            TileConfig tile = Workspace.Instance.Current;
            bool has = tile != null;
            EnableToggle.IsEnabled = has;
            PathBox.IsEnabled = has;
            CheckButton.IsEnabled = has;
            OffsetX.IsEnabled = has;
            OffsetY.IsEnabled = has;
            if (!has)
            {
                PathBox.Text = "";
                Preview.Source = null;
                Hint.Text = "没有磁贴";
                return;
            }

            EnableToggle.IsOn = tile.IconEnabled;
            string source = SourcePath(tile);
            PathBox.Text = source;
            OffsetX.Value = tile.IconOffsetX;
            OffsetY.Value = tile.IconOffsetY;
            ShowPreview(source);
            Hint.Text = "";
        }

        private static string SourcePath(TileConfig tile)
        {
            if (tile == null)
            {
                return "";
            }
            if (!string.IsNullOrWhiteSpace(tile.IconPath))
            {
                return tile.IconPath;
            }
            return tile.Shortcut ?? "";
        }

        private void OnToggle(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null || EnableToggle == null)
            {
                return;
            }
            tile.IconEnabled = EnableToggle.IsOn;
            string source = SourcePath(tile);
            if (tile.IconEnabled && string.IsNullOrWhiteSpace(tile.IconPath) && source.Length > 0)
            {
                tile.IconPath = source;
                PathBox.Text = source;
            }
            Workspace.Instance.Touch();
        }

        private void OnCheck(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }
            string source = PathBox.Text;
            if (string.IsNullOrWhiteSpace(source) || !File.Exists(source))
            {
                Hint.Text = "图标路径不存在";
                return;
            }
            tile.IconPath = source;
            tile.IconEnabled = true;
            tile.IconOffsetX = ToInt(OffsetX, 0);
            tile.IconOffsetY = ToInt(OffsetY, 0);
            EnableToggle.IsOn = true;
            ShowPreview(source);
            Workspace.Instance.MarkDirty();
            Workspace.Instance.Touch();
            Hint.Text = "";
        }

        private async void OnPick(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }

            var picker = new FileOpenPicker();
            WinRT.Interop.InitializeWithWindow.Initialize(picker, App.WindowHandle);
            picker.SuggestedStartLocation = PickerLocationId.ComputerFolder;
            foreach (string extension in IconExtensions)
            {
                picker.FileTypeFilter.Add(extension);
            }
            var file = await picker.PickSingleFileAsync();
            if (file == null)
            {
                return;
            }

            tile.IconPath = file.Path;
            PathBox.Text = file.Path;
            ShowPreview(file.Path);
            Workspace.Instance.MarkDirty();
            Workspace.Instance.Touch();
            Hint.Text = "";
        }

        private void OnApply(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }
            tile.IconEnabled = EnableToggle.IsOn;
            tile.IconOffsetX = ToInt(OffsetX, 0);
            tile.IconOffsetY = ToInt(OffsetY, 0);
            if (!string.IsNullOrWhiteSpace(PathBox.Text) && PathBox.Text != SourcePath(tile))
            {
                tile.IconPath = PathBox.Text;
            }
            Workspace.Instance.MarkDirty();
            Workspace.Instance.Touch();
        }

        private void ShowPreview(string path)
        {
            Preview.Source = null;
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                return;
            }

            string extension = Path.GetExtension(path).ToLowerInvariant();
            if (extension == ".lnk")
            {
                path = Core.ShortcutTarget(path);
                if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
                {
                    return;
                }
                extension = Path.GetExtension(path).ToLowerInvariant();
            }
            if (extension == ".exe" || extension == ".dll" || extension == ".ico")
            {
                string target = Path.Combine(Workspace.Instance.TempDir, "icon_preview.png");
                if (Core.kt_image_extract_icon(Core.Utf8(path), 0, 64, Core.Utf8(target)) != 0)
                {
                    return;
                }
                path = target;
            }

            try
            {
                Preview.Source = new BitmapImage
                {
                    CreateOptions = BitmapCreateOptions.IgnoreImageCache,
                    UriSource = new Uri(path)
                };
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
            }
        }

        private static int ToInt(NumberBox box, int fallback)
        {
            return double.IsNaN(box.Value) ? fallback : (int)Math.Round(box.Value);
        }
    }
}
