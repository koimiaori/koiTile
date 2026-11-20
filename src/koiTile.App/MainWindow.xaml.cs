using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using koiTile.Interop;
using koiTile.Models;
using koiTile.Pages;
using koiTile.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Windows.Foundation;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Storage.Pickers;
using WinRT;

namespace koiTile
{
    public sealed partial class MainWindow : Window
    {
        private Windows.UI.ViewManagement.UISettings _settings;
        private Microsoft.UI.Dispatching.DispatcherQueueTimer _themeTimer;

        public MainWindow()
        {
            InitializeComponent();
            Title = "koiTile";
            ExtendsContentIntoTitleBar = true;
            _settings = new Windows.UI.ViewManagement.UISettings();
            _settings.ColorValuesChanged += OnSystemColorChanged;
            ApplySystemTheme();

            IntPtr handle = WinRT.Interop.WindowNative.GetWindowHandle(this);
            App.WindowHandle = handle;

            var windowId = Microsoft.UI.Win32Interop.GetWindowIdFromWindow(handle);
            var appWindow = Microsoft.UI.Windowing.AppWindow.GetFromWindowId(windowId);
            appWindow.Resize(new Windows.Graphics.SizeInt32(1100, 700));
            try
            {
                string icon = Path.Combine(AppContext.BaseDirectory, "Assets", "koiTile.ico");
                if (File.Exists(icon))
                {
                    appWindow.SetIcon(icon);
                }
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
            }

            Workspace.Instance.Start();
            Workspace.Instance.PreviewChanged += OnPreviewChanged;
            Workspace.Instance.CurrentChanged += OnCurrentChanged;
            Workspace.Instance.LocalTileOpened += OnLocalTileOpened;

            Board.Items = Workspace.Instance.Tiles;
            Board.SelectionChanged += OnBoardSelectionChanged;
            Board.ViewChanged += OnBoardViewChanged;


            Nav.SelectedItem = Nav.MenuItems[0];
            Board.Select(Workspace.Instance.Current);
            Board.Refresh();
            Scheduler.Instance.Start(DispatcherQueue);

            _themeTimer = DispatcherQueue.CreateTimer();
            _themeTimer.Interval = TimeSpan.FromSeconds(1);
            _themeTimer.Tick += OnThemeTick;
            _themeTimer.Start();

            Closed += OnAutoSave;
            HomeStore.Load();
            ShowHome();
        }

        private void OnSystemColorChanged(Windows.UI.ViewManagement.UISettings sender, object args)
        {
            DispatcherQueue.TryEnqueue(ApplySystemTheme);
        }

        private void OnThemeTick(Microsoft.UI.Dispatching.DispatcherQueueTimer sender, object args)
        {
            ApplySystemTheme();
        }

        private void ApplySystemTheme()
        {
            try
            {
                bool darkNow = SystemDark();
                ElementTheme theme = darkNow ? ElementTheme.Dark : ElementTheme.Light;
                if (RootGrid.RequestedTheme == theme)
                {
                    return;
                }
                bool dark = theme == ElementTheme.Dark;
                ApplyDarkFrame(dark);
                RootGrid.RequestedTheme = theme;
                Board.ApplyTheme(dark);
                Log.Write("theme: " + theme);
            }
            catch (Exception ex)
            {
                Log.Write("theme switch failed: " + ex.Message);
            }
        }

        [System.Runtime.InteropServices.DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);

        private static bool SystemDark()
        {
            try
            {
                using (Microsoft.Win32.RegistryKey key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                    @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"))
                {
                    object value = key == null ? null : key.GetValue("AppsUseLightTheme");
                    if (value is int light)
                    {
                        return light == 0;
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("theme probe failed: " + ex.Message);
            }
            return false;
        }

        private static Windows.UI.Color ThemeColor(bool dark)        {
            return dark
                ? Windows.UI.Color.FromArgb(255, 0x1F, 0x1F, 0x1F)
                : Windows.UI.Color.FromArgb(255, 0xE4, 0xE4, 0xE4);
        }

        private void ApplyDarkFrame(bool dark)
        {
            if (App.WindowHandle == IntPtr.Zero)
            {
                return;
            }
            int value = dark ? 1 : 0;
            DwmSetWindowAttribute(App.WindowHandle, 20, ref value, sizeof(int));
        }

        private void OnNavSelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
        {
            if (args.SelectedItem is NavigationViewItem item)
            {
                string tag = item.Tag as string;
                Type page = tag switch
                {
                    "info" => typeof(InfoPage),
                    "icon" => typeof(IconPage),

                    _ => typeof(ImagePage)
                };
                PageHost.Navigate(page);
                LoadCurrentPage();
            }
        }

        private void LoadCurrentPage()
        {
            if (PageHost.Content is ITilePage page)
            {
                page.LoadTile();
            }
        }

        private string _openedTileId = "";

        private void OnGuidePreviewToggled(object sender, RoutedEventArgs e)
        {
            Board.Guide = SizeGuideToggle.IsOn && GuidePreviewToggle.IsOn;
            Workspace.Instance.Guide = Board.Guide;
        }

        private void OnCurrentChanged()
        {
            LoadCurrentPage();
            UpdateStartButton();
            UpdatePreviewStrip();
            TileConfig current = Workspace.Instance.Current;
            if (current != null && !ReferenceEquals(Board.Selected, current))
            {
                Board.Select(current);
                Board.Refresh();
            }
        }

        private void OnLocalTileOpened(TileConfig tile)
        {
            if (tile == null)
            {
                return;
            }
            _openedTileId = tile.Id;
            Board.Select(tile);
            Board.Refresh();
            Nav.SelectedItem = Nav.MenuItems[1];
            UpdateStartButton();
        }

        private void UpdateStartButton()
        {
            if (StartButton == null)
            {
                return;
            }
            TileConfig current = Workspace.Instance.Current;
            bool overwrite = current != null && current.Id.Length > 0 && current.Id == _openedTileId;
            StartButton.Content = overwrite ? "覆盖并保存" : "添加至开始菜单";
        }

        private void OnPreviewChanged()
        {
            Board.Refresh();
            UpdatePreviewStrip();
        }

        private void OnBoardSelectionChanged()
        {
            Workspace.Instance.Select(Board.Selected);
        }

        private void OnSizeSmall(object sender, RoutedEventArgs e)
        {
            OnBoardSetSize(0);
        }

        private void OnSizeMedium(object sender, RoutedEventArgs e)
        {
            OnBoardSetSize(1);
        }

        private void OnSizeWide(object sender, RoutedEventArgs e)
        {
            OnBoardSetSize(2);
        }

        private void OnSizeLarge(object sender, RoutedEventArgs e)
        {
            OnBoardSetSize(3);
        }

        private void OnBoardSetSize(int size)
        {
            if (Board.Selected == null)
            {
                ShowError("请先选择一个磁贴");
                return;
            }
            if (TileGrid.SetSize(Workspace.Instance.Tiles, Board.Selected, size, Workspace.Instance.NextId))
            {
                AfterGridChanged(Board.Selected);
            }
        }

        private void AfterGridChanged(TileConfig tile)
        {
            Workspace.Instance.MarkDirty();
            Workspace.Instance.NotifyListChanged();
            Board.Refresh();
            Board.Select(tile);
        }
        private void ShowInfo(string message)
        {
            ShowBar(message, InfoBarSeverity.Success);
        }

        private void ShowWarning(string message)
        {
            ShowBar(message, InfoBarSeverity.Warning);
        }

        private void ShowError(string message)
        {
            ShowBar(message, InfoBarSeverity.Error);
        }

        private Microsoft.UI.Dispatching.DispatcherQueueTimer _barTimer;

        private void ShowBar(string message, InfoBarSeverity severity)
        {
            if (MessageBar == null)
            {
                return;
            }
            MessageBar.Severity = severity;
            MessageBar.Message = message;
            MessageBar.IsOpen = true;
            if (_barTimer == null)
            {
                _barTimer = DispatcherQueue.CreateTimer();
                _barTimer.Interval = TimeSpan.FromSeconds(5);
                _barTimer.IsRepeating = false;
                _barTimer.Tick += OnBarTick;
            }
            _barTimer.Stop();
            _barTimer.Start();
        }

        private void OnBarTick(Microsoft.UI.Dispatching.DispatcherQueueTimer sender, object args)
        {
            sender.Stop();
            if (MessageBar != null)
            {
                MessageBar.IsOpen = false;
            }
        }
        private void OnAutoSave(object sender, WindowEventArgs args)
        {
            Scheduler.Instance.Sync();
            Workspace.Instance.SaveAll();
            HomeStore.Capture(_editingEntry);
        }

        private int _mode = 1;
        private HomeEntry _editingEntry;
        private bool _autoCropSuppress;

        private void ShowHome()
        {
            AppTitleBar.IsBackButtonVisible = false;
            AppTitleBar.IsPaneToggleButtonVisible = false;
            HomeRoot.Visibility = Visibility.Visible;
            EditorRoot.Visibility = Visibility.Collapsed;
            RefreshHome();
        }

        private void ShowEditor()
        {
            HomeRoot.Visibility = Visibility.Collapsed;
            EditorRoot.Visibility = Visibility.Visible;
            AppTitleBar.IsBackButtonVisible = true;
            AppTitleBar.IsPaneToggleButtonVisible = true;
        }

        private void EnterMode(int mode)
        {
            _mode = mode;
            Workspace.Instance.SetMode(mode);
            bool single = mode == 0;
            Board.SingleTile = single;
            Board.Items = Workspace.Instance.Tiles;
            PreviewPanel.Visibility = single ? Visibility.Visible : Visibility.Collapsed;
            AutoCropToggle.Visibility = single ? Visibility.Visible : Visibility.Collapsed;
            SizeGuideToggle.Visibility = single ? Visibility.Collapsed : Visibility.Visible;
            GuidePreviewToggle.Visibility = single || !SizeGuideToggle.IsOn ? Visibility.Collapsed : Visibility.Visible;
            Board.Guide = !single && SizeGuideToggle.IsOn && GuidePreviewToggle.IsOn;
            Workspace.Instance.Guide = Board.Guide;
            SizeButtons.Visibility = single ? Visibility.Collapsed : Visibility.Visible;
            UpdateSizeButtons();
            TileConfig current = Workspace.Instance.Current;
            _autoCropSuppress = true;
            AutoCropToggle.IsOn = current == null || current.AutoCrop;
            _autoCropSuppress = false;
            if (single)
            {
                if (!UwpToggle.IsOn && current != null && (current.PreviewSize == 2 || current.PreviewSize == 3))
                {
                    Workspace.Instance.ApplyPreviewSize(1);
                }
                Workspace.Instance.ApplyCrop();
            }
            SyncView();
        }

        private void SyncView()
        {
            Board.Select(Workspace.Instance.Current);
            Board.ReloadView();
            RebuildPage();
            UpdateStartButton();
            UpdatePreviewStrip();
        }

        private void RebuildPage()
        {
            if (PageHost == null)
            {
                return;
            }
            string tag = Nav.SelectedItem is NavigationViewItem item ? item.Tag as string : "background";
            Type page = tag switch
            {
                "info" => typeof(InfoPage),
                "icon" => typeof(IconPage),

                _ => typeof(ImagePage)
            };
            PageHost.Navigate(page);
            LoadCurrentPage();
        }

        private void RefreshHome()
        {
            if (TileList == null || GroupList == null)
            {
                return;
            }
            HomeStore.PruneUninstalled();
            var tiles = new List<HomeEntry>();
            var groups = new List<HomeEntry>();
            foreach (HomeEntry entry in HomeStore.Entries)
            {
                if (entry.Kind == 0)
                {
                    tiles.Add(entry);
                }
                else
                {
                    groups.Add(entry);
                }
            }
            TileList.ItemsSource = tiles;
            GroupList.ItemsSource = groups;
        }

        private void OnAddTileEntry(object sender, RoutedEventArgs e)
        {
            HomeStore.Capture(_editingEntry);
            _editingEntry = null;
            EnterMode(0);
            ShowEditor();
        }

        private void OnAddGroupEntry(object sender, RoutedEventArgs e)
        {
            HomeStore.Capture(_editingEntry);
            _editingEntry = null;
            EnterMode(1);
            ShowEditor();
        }


        private void OnTitleBarBackRequested(TitleBar sender, object args)
        {
            GoHome();
        }

        private void OnPaneToggleRequested(TitleBar sender, object args)
        {
            Nav.IsPaneOpen = !Nav.IsPaneOpen;
        }

        private void GoHome()
        {
            HomeStore.Capture(_editingEntry);
            _editingEntry = null;
            Workspace.Instance.ResetDocument();
            Board.Select(Workspace.Instance.Current);
            Board.ReloadView();
            UpdatePreviewStrip();
            ShowHome();
        }

        private void OnTileEntryClick(object sender, ItemClickEventArgs e)
        {
            HomeEntry entry = e.ClickedItem as HomeEntry;
            if (entry == null)
            {
                return;
            }
            if (_editingEntry != null && !ReferenceEquals(_editingEntry, entry))
            {
                HomeStore.Capture(_editingEntry);
            }
            EnterMode(entry.Kind);
            _editingEntry = entry;
            Workspace.Instance.RestoreSnapshot(entry.Tiles, entry.BoardImage, entry.BoardZoom, entry.BoardPanX, entry.BoardPanY);
            if (entry.Kind == 0)
            {
                Workspace.Instance.SaveCropAll();
            }
            SyncView();
            ShowEditor();
        }

        private void OnEntryPointerEntered(object sender, PointerRoutedEventArgs e)
        {
            SetRemoveVisible(sender as FrameworkElement, Visibility.Visible);
        }

        private void OnEntryPointerExited(object sender, PointerRoutedEventArgs e)
        {
            SetRemoveVisible(sender as FrameworkElement, Visibility.Collapsed);
        }

        private static void SetRemoveVisible(FrameworkElement element, Visibility visibility)
        {
            if (element == null)
            {
                return;
            }
            Button button = element.FindName("RemoveButton") as Button;
            if (button != null)
            {
                button.Visibility = visibility;
            }
        }

        private async void OnRemoveEntry(object sender, RoutedEventArgs e)
        {
            Button button = sender as Button;
            HomeEntry entry = button == null ? null : button.Tag as HomeEntry;
            if (entry == null)
            {
                return;
            }
            var dialog = new ContentDialog
            {
                XamlRoot = RootGrid.XamlRoot,
                Content = entry.Kind == 0 ? "删除该磁贴" : "删除该磁贴组",
                PrimaryButtonText = "是",
                CloseButtonText = "否"
            };
            ContentDialogResult result;
            try
            {
                result = await dialog.ShowAsync();
            }
            catch (Exception ex)
            {
                Log.Write("remove dialog: " + ex.Message);
                return;
            }
            if (result != ContentDialogResult.Primary)
            {
                return;
            }
            foreach (string id in entry.TileIds)
            {
                if (entry.Install == 1)
                {
                    TilePackage.RemoveById(id);
                }
                else
                {
                    Workspace.Instance.RemoveInstalled(id);
                }
            }
            HomeStore.Remove(entry);
            RefreshHome();
        }
        private void OnBoardViewChanged()
        {
            UpdatePreviewStrip();
        }

        private void OnPreviewStripPressed(object sender, PointerRoutedEventArgs e)
        {
            if (Workspace.Instance.Mode != 0)
            {
                return;
            }
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            Point point = e.GetCurrentPoint(PreviewPanel).Position;
            int size = PreviewSizeAt(point.X, point.Y);
            if (size < 0 || tile.PreviewSize == size)
            {
                return;
            }
            if (!UwpToggle.IsOn && (size == 2 || size == 3))
            {
                return;
            }
            SelectPreviewSize(size);
        }

        private static int PreviewSizeAt(double x, double y)
        {
            if (y <= 204 && x >= 38 && x <= 242)
            {
                return 3;
            }
            if (y <= 204 && x >= 246 && x <= 450)
            {
                if (y >= 104)
                {
                    return 2;
                }
                if (x <= 346)
                {
                    return 1;
                }
                if (y <= 48 && x >= 350 && x <= 398)
                {
                    return 0;
                }
                return -1;
            }
            return -1;
        }

        private void OnPreviewSlotTapped(object sender, TappedRoutedEventArgs e)
        {
            if (Workspace.Instance.Mode != 0)
            {
                return;
            }
            TileConfig tile = Workspace.Instance.Current;
            FrameworkElement element = sender as FrameworkElement;
            if (tile == null || element == null)
            {
                return;
            }
            if (!(element.Tag is string tag) || !int.TryParse(tag, out int size))
            {
                return;
            }
            if (!UwpToggle.IsOn && (size == 2 || size == 3))
            {
                return;
            }
            if (tile.PreviewSize == size)
            {
                return;
            }
            SelectPreviewSize(size);
        }

        private void OnUwpToggled(object sender, RoutedEventArgs e)
        {
            UpdateSizeButtons();
            if (Workspace.Instance.Mode != 0)
            {
                return;
            }
            if (!UwpToggle.IsOn)
            {
                TileConfig current = Workspace.Instance.Current;
                if (current != null && (current.PreviewSize == 2 || current.PreviewSize == 3))
                {
                    SelectPreviewSize(1);
                }
            }
            UpdatePreviewStrip();
        }

        private void OnSizeGuideToggled(object sender, RoutedEventArgs e)
        {
            GuidePreviewToggle.Visibility = SizeGuideToggle.IsOn ? Visibility.Visible : Visibility.Collapsed;
            Board.Guide = SizeGuideToggle.IsOn && GuidePreviewToggle.IsOn;
            Workspace.Instance.Guide = Board.Guide;
        }

        private void UpdateSizeButtons()
        {
            if (SizeWideButton == null || SizeLargeButton == null)
            {
                return;
            }
            bool uwp = UwpToggle == null || UwpToggle.IsOn;
            SizeWideButton.IsEnabled = uwp;
            SizeLargeButton.IsEnabled = uwp;
        }

        private void SelectPreviewSize(int size)
        {
            Workspace.Instance.SaveCrop();
            Workspace.Instance.ApplyPreviewSize(size);
            Workspace.Instance.MarkDirty();
            Workspace.Instance.ApplyCrop();
            Workspace.Instance.NotifyListChanged();
            TileConfig tile = Workspace.Instance.Current;
            Board.Select(tile);
            Board.ReloadView();
            UpdatePreviewStrip();
        }

        private void OnAutoCropToggled(object sender, RoutedEventArgs e)
        {
            if (_autoCropSuppress || Workspace.Instance.Mode != 0)
            {
                return;
            }
            TileConfig tile = Workspace.Instance.Current;
            if (tile == null)
            {
                return;
            }
            if (!AutoCropToggle.IsOn)
            {
                Workspace.Instance.SaveCropAll();
            }
            tile.AutoCrop = AutoCropToggle.IsOn;
            Workspace.Instance.ApplyCrop();
            Workspace.Instance.MarkDirty();
            Board.ReloadView();
            UpdatePreviewStrip();
        }

        private string _previewPath = "";
        private int _previewWidth;
        private int _previewHeight;
        private ImageSource _previewSource;

        private void UpdatePreviewStrip()
        {
            if (PreviewLarge == null || PreviewWide == null)
            {
                return;
            }
            if (Workspace.Instance.Mode != 0)
            {
                return;
            }
            string path = Workspace.Instance.BackgroundImage ?? "";
            bool hasImage = path.Length > 0 && EnsurePreviewImage(path);
            double cover = hasImage
                ? Math.Max(TileGrid.SpanPixels(4) / (double)_previewWidth, TileGrid.SpanPixels(4) / (double)_previewHeight)
                : 0;
            ImageSource source = hasImage ? _previewSource : null;
            TileConfig tile = Workspace.Instance.Current;
            bool dark = Board.IsDark;
            bool uwp = UwpToggle == null || UwpToggle.IsOn;
            if (uwp)
            {
                PreviewLarge.Render(tile, 3, 204, 204, dark, source, _previewWidth, _previewHeight, cover);
                PreviewWide.Render(tile, 2, 204, 100, dark, source, _previewWidth, _previewHeight, cover);
            }
            else
            {
                PreviewLarge.Clear();
                PreviewWide.Clear();
            }
            PreviewMedium.Render(tile, 1, 100, 100, dark, source, _previewWidth, _previewHeight, cover);
            PreviewSmall.Render(tile, 0, 48, 48, dark, source, _previewWidth, _previewHeight, cover);
            UpdatePreviewRings();
        }

        private bool EnsurePreviewImage(string path)
        {
            if (path == _previewPath && _previewSource != null)
            {
                return true;
            }
            _previewSource = null;
            _previewWidth = 0;
            _previewHeight = 0;
            _previewPath = "";
            int width = 0;
            int height = 0;
            if (Core.kt_image_info(Core.Utf8(path), ref width, ref height) != 0 || width <= 0 || height <= 0)
            {
                return false;
            }
            try
            {
                _previewSource = new BitmapImage
                {
                    CreateOptions = BitmapCreateOptions.IgnoreImageCache,
                    UriSource = new Uri(path)
                };
            }
            catch (Exception ex)
            {
                Log.Write("preview strip: " + ex.Message);
                return false;
            }
            _previewPath = path;
            _previewWidth = width;
            _previewHeight = height;
            return true;
        }

        private void UpdatePreviewRings()
        {
            if (RingLarge == null)
            {
                return;
            }
            TileConfig tile = Workspace.Instance.Current;
            int size = tile == null ? -1 : tile.PreviewSize;
            Brush accent = AccentBrush();
            Brush none = new SolidColorBrush(Windows.UI.Color.FromArgb(0, 0, 0, 0));
            bool uwp = UwpToggle == null || UwpToggle.IsOn;
            RingLarge.BorderBrush = uwp && size == 3 ? accent : none;
            RingMedium.BorderBrush = size == 1 ? accent : none;
            RingSmall.BorderBrush = size == 0 ? accent : none;
            RingWide.BorderBrush = uwp && size == 2 ? accent : none;
        }

        private static Brush AccentBrush()
        {
            try
            {
                if (Application.Current.Resources["AccentFillColorDefaultBrush"] is Brush brush)
                {
                    return brush;
                }
            }
            catch (Exception ex)
            {
                Log.Write(ex.Message);
            }
            return new SolidColorBrush(Windows.UI.Color.FromArgb(255, 0, 120, 212));
        }

        private async System.Threading.Tasks.Task<string> CapturePreview()
        {
            try
            {
                Board.SetChrome(false);
                var target = new Microsoft.UI.Xaml.Media.Imaging.RenderTargetBitmap();
                await target.RenderAsync(Board);
                var pixels = await target.GetPixelsAsync();
                Directory.CreateDirectory(HomeStore.PreviewDir);
                string path = Path.Combine(HomeStore.PreviewDir, DateTime.Now.Ticks.ToString() + ".png");
                using (FileStream stream = File.Create(path))
                {
                    Windows.Graphics.Imaging.BitmapEncoder encoder = await Windows.Graphics.Imaging.BitmapEncoder.CreateAsync(Windows.Graphics.Imaging.BitmapEncoder.PngEncoderId, stream.AsRandomAccessStream());
                    encoder.SetPixelData(Windows.Graphics.Imaging.BitmapPixelFormat.Bgra8, Windows.Graphics.Imaging.BitmapAlphaMode.Premultiplied, (uint)target.PixelWidth, (uint)target.PixelHeight, 96, 96, pixels.ToArray());
                    await encoder.FlushAsync();
                }
                return path;
            }
            catch (Exception ex)
            {
                Log.Write("preview: " + ex.Message);
                return "";
            }
            finally
            {
                Board.SetChrome(true);
            }
        }
        private static string NextFreeTileId(System.Collections.Generic.HashSet<string> assigned)
        {
            for (int index = 1; ; index++)
            {
                string id = "tile-" + index.ToString();
                if (TilePackage.IsInstalled(id) || assigned.Contains(id))
                {
                    continue;
                }
                bool used = false;
                foreach (HomeEntry entry in HomeStore.Entries)
                {
                    if (entry.TileIds != null && entry.TileIds.IndexOf(id) >= 0)
                    {
                        used = true;
                        break;
                    }
                }
                if (!used)
                {
                    return id;
                }
            }
        }

        private async void OnAddToStart(object sender, RoutedEventArgs e)
        {
            if (Workspace.Instance.Tiles.Count == 0)
            {
                ShowError("没有磁贴");
                return;
            }
            ITilePage page = PageHost.Content as ITilePage;
            if (page != null)
            {
                page.Flush();
            }
            if (_editingEntry == null)
            {
                var assigned = new System.Collections.Generic.HashSet<string>();
                foreach (TileConfig tile in Workspace.Instance.Tiles)
                {
                    if (TilePackage.IsInstalled(tile.Id) || assigned.Contains(tile.Id))
                    {
                        tile.Id = NextFreeTileId(assigned);
                    }
                    assigned.Add(tile.Id);
                }
                Workspace.Instance.FlushAll();
            }
            int ok = 0;
            int failed = 0;
            bool uwp = UwpToggle.IsOn;
            Workspace.Instance.FlushAll();
            foreach (TileConfig tile in Workspace.Instance.Tiles)
            {
                if (string.IsNullOrWhiteSpace(tile.Shortcut) && !uwp)
                {
                    failed++;
                    continue;
                }
                bool done = uwp ? TilePackage.Install(tile) : (tile.LaunchType == 0 && Workspace.Instance.InstallStart(tile));
                if (done)
                {
                    ok++;
                    continue;
                }
                failed++;
            }
            if (ok > 0)
            {
                string preview = await CapturePreview();
                if (_editingEntry != null)
                {
                    HomeStore.Update(_editingEntry, preview, uwp ? 1 : 0);
                }
                else
                {
                    HomeStore.Add(_mode, preview, uwp ? 1 : 0);
                }
            }
            if (ok == 0)
            {
                string reason = Core.LastError();
                ShowError(string.IsNullOrWhiteSpace(reason) ? "添加失败" : "添加失败：" + reason);
            }
            else if (failed == 0)
            {
                if (uwp)
                {
                    TileConfig current = Workspace.Instance.Current;
                    string name = current == null ? "" : (current.AppName ?? "").Trim();
                    ShowInfo(name.Length == 0 ? "已添加" : "已添加" + name);
                }
                else
                {
                    ShowInfo("已添加到开始菜单");
                }
            }
            else
            {
                ShowWarning("部分已添加");
            }
            if (ok > 0)
            {
                Workspace.Instance.ResetDocument();
                Board.Select(Workspace.Instance.Current);
                Board.ReloadView();
                UpdatePreviewStrip();
                ShowHome();
            }
        }

        private async void OnRestore(object sender, RoutedEventArgs e)
        {
            var dialog = new ContentDialog
            {
                XamlRoot = Board.XamlRoot,
                Title = "恢复默认",
                Content = "清除koiTile生成的内容",
                PrimaryButtonText = "确定",
                CloseButtonText = "取消"
            };
            if (await dialog.ShowAsync() != ContentDialogResult.Primary)
            {
                return;
            }
            Workspace.Instance.ResetCurrentTile();
            Board.Refresh();
            RebuildPage();
            UpdatePreviewStrip();
            ShowInfo("已恢复默认");
        }
    }
}
