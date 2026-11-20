using System;
using System.Collections.Generic;
using koiTile.Interop;
using koiTile.Models;
using koiTile.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.UI;

namespace koiTile.Pages
{
    public sealed partial class ImagePage : Page, ITilePage
    {
        private static readonly string[] ImageExtensions = { ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif" };
        private static Color _themeColor;
        private static bool _themeLoaded;

        private bool _loading;
        private string _signature = "";

        public ImagePage()
        {
            InitializeComponent();
            Loaded += (s, e) => LoadTile();
            Workspace.Instance.BackgroundChanged += LoadTile;
        }

        public void LoadTile()
        {
            TileConfig tile = Workspace.Instance.Current;
            bool has = tile != null;
            AppNameBox.IsEnabled = has;
            ShowNameToggle.IsEnabled = has;
            NumberToggle.IsEnabled = has;
            FreeToggle.IsEnabled = has;
            FreeToggle.IsOn = Workspace.Instance.FreePosition;
            ModeBox.IsEnabled = has;
            PlanMinutesBox.IsEnabled = has;
            ImageList.IsEnabled = has;
            if (has)
            {
                AppNameBox.Text = tile.AppName;
                ShowNameToggle.IsOn = tile.ShowName;
                NumberToggle.IsOn = tile.NumberAll;
            }
            Picker.IsEnabled = has;
            ApplyButton.IsEnabled = has;
            if (!has)
            {
                Hint.Text = "没有磁贴";
                return;
            }

            string current = Workspace.Instance.BackgroundImage ?? "";
            if (tile.Images.Count == 0 && current.Length > 0)
            {
                tile.Images.Add(current);
            }

            _loading = true;
            ModeBox.SelectedIndex = ModeIndex(tile.UpdateMode);
            SetModeRow(tile.UpdateMode);
            PlanMinutesBox.Value = tile.PlanMinutes < 1 ? 30 : tile.PlanMinutes;
            SyncImages(current);
            Picker.Color = tile.ColorEnabled ? ToColor(tile.Color) : ThemeColor();
            Hint.Text = "";
            _loading = false;
        }

        private void SyncImages(string current)
        {
            var paths = new List<string>();
            int selected = -1;
            foreach (string path in Workspace.Instance.Images)
            {
                if (path == current && selected < 0)
                {
                    selected = paths.Count;
                }
                paths.Add(path);
            }
            string signature = string.Join("|", paths);
            if (signature != _signature)
            {
                _signature = signature;
                var items = new List<PeekItem>();
                foreach (string path in paths)
                {
                    items.Add(new PeekItem(path));
                }
                ImageList.ItemsSource = items;
            }
            if (ImageList.SelectedIndex != selected)
            {
                ImageList.SelectedIndex = selected;
            }
        }

        private void OnImageSelected(object sender, SelectionChangedEventArgs e)
        {
            if (_loading)
            {
                return;
            }
            if (ImageList.SelectedItem is PeekItem item)
            {
                string path = item.Path;
                DispatcherQueue.TryEnqueue(() => Workspace.Instance.SelectImage(path));
            }
        }

        private void OnModeChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_loading)
            {
                return;
            }
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.UpdateMode = ModeFromIndex(ModeBox.SelectedIndex);
            SetModeRow(tile.UpdateMode);
            Workspace.Instance.MarkDirty();
        }

        private void SetModeRow(int mode)
        {
            bool plan = mode == 2;
            PlanMinutesBox.Visibility = plan ? Visibility.Visible : Visibility.Collapsed;
            ModeBox.CornerRadius = plan ? new CornerRadius(4, 0, 0, 4) : new CornerRadius(4);
        }

        private void OnPlanMinutesChanged(NumberBox sender, NumberBoxValueChangedEventArgs args)
        {
            if (args.NewValue < 1)
            {
                sender.Value = 1;
            }
            if (_loading)
            {
                return;
            }
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.PlanMinutes = double.IsNaN(sender.Value) ? 30 : (int)Math.Round(sender.Value);
            Workspace.Instance.MarkDirty();
        }

        private static int ModeIndex(int mode)
        {
            if (mode == 1)
            {
                return 4;
            }
            if (mode == 2)
            {
                return 3;
            }
            return 0;
        }

        private static int ModeFromIndex(int index)
        {
            if (index == 3)
            {
                return 2;
            }
            if (index == 4)
            {
                return 1;
            }
            return 0;
        }

        private void OnAppNameChanged(object sender, TextChangedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.AppName = AppNameBox.Text;
            Workspace.Instance.Touch();
        }

        private void OnShowNameToggled(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.ShowName = ShowNameToggle.IsOn;
            Workspace.Instance.Touch();
        }

        private void OnNumberToggled(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            tile.NumberAll = NumberToggle.IsOn;
            Workspace.Instance.Touch();
        }

        private void OnFreeToggled(object sender, RoutedEventArgs e)
        {
            Workspace.Instance.FreePosition = FreeToggle.IsOn;
            Workspace.Instance.RefreshView();
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
            try
            {
                WinRT.Interop.InitializeWithWindow.Initialize(picker, App.WindowHandle);
                picker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;
                foreach (string extension in ImageExtensions)
                {
                    picker.FileTypeFilter.Add(extension);
                }
                var files = await picker.PickMultipleFilesAsync();
                if (files == null || files.Count == 0)
                {
                    return;
                }
                var paths = new List<string>();
                foreach (StorageFile file in files)
                {
                    int width = 0;
                    int height = 0;
                    if (Core.kt_image_info(Core.Utf8(file.Path), ref width, ref height) != 0 || width <= 0 || height <= 0)
                    {
                        continue;
                    }
                    paths.Add(file.Path);
                }
                if (paths.Count == 0)
                {
                    Hint.Text = "图片无法读取";
                    return;
                }
                bool first = tile.Images.Count == 0;
                foreach (string path in paths)
                {
                    Workspace.Instance.AddImage(path);
                }
                if (first)
                {
                    ClearColors();
                }
                LoadTile();
                Hint.Text = "";
            }
            catch (Exception ex)
            {
                Log.Write("pick failed: " + ex);
                Hint.Text = "选择图片失败";
            }
        }

        private void OnApply(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }

            tile.ColorEnabled = true;
            tile.Color = ToHex(Picker.Color);
            Workspace.Instance.Touch();
        }

        private void OnReset(object sender, RoutedEventArgs e)
        {
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                Hint.Text = "没有磁贴";
                return;
            }

            tile.ColorEnabled = false;
            tile.Color = ToHex(ThemeColor());
            Workspace.Instance.Touch();
            Hint.Text = "已恢复";
        }

        private void ClearColors()
        {
            bool changed = false;
            foreach (TileConfig item in Workspace.Instance.Tiles)
            {
                if (item.ColorEnabled || item.Color.Length > 0)
                {
                    item.ColorEnabled = false;
                    item.Color = "";
                    changed = true;
                }
            }
            if (changed)
            {
                Workspace.Instance.Touch();
            }
        }

        private static Color ToColor(string hex)
        {
            if (string.IsNullOrEmpty(hex) || hex.Length != 7 || hex[0] != '#')
            {
                return ThemeColor();
            }
            try
            {
                byte r = Convert.ToByte(hex.Substring(1, 2), 16);
                byte g = Convert.ToByte(hex.Substring(3, 2), 16);
                byte b = Convert.ToByte(hex.Substring(5, 2), 16);
                return Color.FromArgb(255, r, g, b);
            }
            catch
            {
                return ThemeColor();
            }
        }

        private static Color ThemeColor()
        {
            if (_themeLoaded)
            {
                return _themeColor;
            }
            _themeLoaded = true;
            try
            {
                var settings = new Windows.UI.ViewManagement.UISettings();
                _themeColor = settings.GetColorValue(Windows.UI.ViewManagement.UIColorType.Accent);
            }
            catch (Exception ex)
            {
                Log.Write("accent failed: " + ex.Message);
                _themeColor = Color.FromArgb(255, 0, 120, 212);
            }
            return _themeColor;
        }

        private static string ToHex(Color color)
        {
            return "#" + color.R.ToString("X2") + color.G.ToString("X2") + color.B.ToString("X2");
        }
    }
}
