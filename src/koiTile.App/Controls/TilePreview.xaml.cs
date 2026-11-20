using System;
using koiTile.Models;
using koiTile.Services;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.UI;

namespace koiTile.Controls
{
    public sealed partial class TilePreview : UserControl
    {

        private readonly Image _picture;
        private readonly Image _icon;

        public TilePreview()
        {
            InitializeComponent();
            _picture = new Image { Stretch = Stretch.Fill, IsHitTestVisible = false };
            PictureLayer.Children.Add(_picture);
            _icon = new Image { Stretch = Stretch.Uniform, IsHitTestVisible = false };
            IconLayer.Children.Add(_icon);
        }

        internal void Render(TileConfig tile, int size, double width, double height, bool dark, ImageSource source, int imageWidth, int imageHeight, double cover)
        {
            double fit = width / TileGrid.SpanPixels(4);
            bool hasImage = source != null;
            Fill.Background = FillBrush(tile, dark, hasImage);
            Edge.BorderBrush = new SolidColorBrush(dark
                ? Color.FromArgb(0x3D, 0xFF, 0xFF, 0xFF)
                : Color.FromArgb(0x2E, 0x00, 0x00, 0x00));

            _picture.Source = source;
            if (hasImage && tile != null && imageWidth > 0 && imageHeight > 0)
            {
                Workspace.Instance.CropView(size, out int zoom, out int panX, out int panY);
                double scale = cover * (zoom / 100.0) * fit;
                double pictureWidth = imageWidth * scale;
                double pictureHeight = imageHeight * scale;
                _picture.Width = pictureWidth;
                _picture.Height = pictureHeight;
                Canvas.SetLeft(_picture, (width / 2.0) + (panX * fit) - (pictureWidth / 2.0));
                Canvas.SetTop(_picture, (height / 2.0) + (panY * fit) - (pictureHeight / 2.0));
            }

            double inset = Math.Max(2, Math.Round(height * 0.06));
            Caption.Visibility = size == TileGrid.MinSize ? Visibility.Collapsed : Visibility.Visible;
            Caption.FontSize = 12;
            Caption.Margin = new Thickness(inset + 2, 0, inset, inset - 1);
            Caption.MaxWidth = Math.Max(8, width - (inset * 2));
            Caption.Text = tile != null && tile.ShowName ? Workspace.Instance.TileLabel(tile) : "";

            string path = tile == null ? "" : (string.IsNullOrWhiteSpace(tile.IconPath) ? (tile.Shortcut ?? "") : tile.IconPath);
            bool showIcon = tile != null && tile.IconEnabled;
            ImageSource iconSource = showIcon ? TileIcons.Source(path) : null;
            _icon.Source = iconSource;
            _icon.Visibility = iconSource == null ? Visibility.Collapsed : Visibility.Visible;
            if (iconSource == null || tile == null)
            {
                return;
            }
            double iconSize = TileIcons.Size(size);
            _icon.Width = iconSize;
            _icon.Height = iconSize;
            double centerX = (width / 2.0) + tile.IconOffsetX;
            double centerY = (height / 2.0) + tile.IconOffsetY - 2;
            Canvas.SetLeft(_icon, centerX - (iconSize / 2.0));
            Canvas.SetTop(_icon, centerY - (iconSize / 2.0));
        }

        internal void Clear()
        {
            Fill.Background = null;
            _picture.Source = null;
            _icon.Source = null;
            _icon.Visibility = Visibility.Collapsed;
            Caption.Text = "";
        }

        private static Brush FillBrush(TileConfig tile, bool dark, bool hasImage)
        {
            if (tile != null && tile.ColorEnabled)
            {
                return new SolidColorBrush(TileBoard.ParseColor(tile.Color));
            }
            return new SolidColorBrush(Colors.Transparent);
        }
    }
}
