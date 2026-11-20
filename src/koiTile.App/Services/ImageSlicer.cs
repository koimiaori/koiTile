using System;
using System.Collections.Generic;
using koiTile.Interop;
using koiTile.Models;
using Windows.Foundation;

namespace koiTile.Services
{
    internal static class ImageSlicer
    {
        public static bool Slice(TileConfig tile, int size, string path)
        {
            return Slice(tile, size, path, Workspace.Instance.BackgroundImage);
        }

        public static bool Slice(TileConfig tile, int size, string path, string source)
        {
            if (tile == null || string.IsNullOrWhiteSpace(path))
            {
                return false;
            }
            string image = source;
            if (string.IsNullOrWhiteSpace(image))
            {
                return false;
            }
            int imageWidth = 0;
            int imageHeight = 0;
            if (Core.kt_image_info(Core.Utf8(image), ref imageWidth, ref imageHeight) != 0 || imageWidth <= 0 || imageHeight <= 0)
            {
                return false;
            }
            IList<TileConfig> tiles = Workspace.Instance.Tiles;
            Rect cell;
            double centerX;
            double centerY;
            int zoom;
            if (Workspace.Instance.Mode == 0)
            {
                int rows = size == 2 ? 2 : 4;
                cell = new Rect(0, 0, TileGrid.SpanPixels(4), TileGrid.SpanPixels(rows));
                Workspace.Instance.CropView(size, out zoom, out int panX, out int panY);
                centerX = TileGrid.Margin + (cell.Width / 2.0) + panX;
                centerY = TileGrid.Margin + (cell.Height / 2.0) + panY;
            }
            else
            {
                int boardWidth = TileGrid.Width(tiles);
                int boardHeight = TileGrid.Height(tiles);
                if (boardWidth <= 0 || boardHeight <= 0)
                {
                    return false;
                }
                cell = TileGrid.RectOf(tile);
                zoom = Workspace.Instance.BackgroundZoom;
                centerX = TileGrid.Margin + (boardWidth / 2.0) + Workspace.Instance.BackgroundPanX;
                centerY = TileGrid.Margin + (boardHeight / 2.0) + Workspace.Instance.BackgroundPanY;
            }
            double cover = Math.Max((double)Workspace.Instance.ScaleWidth / imageWidth, (double)Workspace.Instance.ScaleHeight / imageHeight);
            double scale = cover * zoom / 100.0;
            double left = centerX - (imageWidth * scale / 2.0);
            double top = centerY - (imageHeight * scale / 2.0);

            int canvasWidth = 0;
            int canvasHeight = 0;
            CanvasSize(size, out canvasWidth, out canvasHeight);
            cell = FitCell(cell, canvasWidth, canvasHeight);
            double cellX = TileGrid.Margin + cell.X;
            double cellY = TileGrid.Margin + cell.Y;
            if (cell.Width <= 0 || cell.Height <= 0)
            {
                return false;
            }
            double ratioX = canvasWidth / cell.Width;
            double ratioY = canvasHeight / cell.Height;

            int drawWidth = Math.Max(1, (int)Math.Round(imageWidth * scale * ratioX));
            int drawHeight = Math.Max(1, (int)Math.Round(imageHeight * scale * ratioY));
            int drawX = (int)Math.Round((left - cellX) * ratioX);
            int drawY = (int)Math.Round((top - cellY) * ratioY);
            if (Core.kt_image_render_slice(Core.Utf8(image), canvasWidth, canvasHeight, drawWidth, drawHeight, drawX, drawY, Core.Utf8(path)) != 0)
            {
                return false;
            }
            return true;
        }

        private static void AddLabel(TileConfig tile, int canvasWidth, int canvasHeight, string path)
        {
            if (!tile.ShowName || canvasWidth <= 80)
            {
                return;
            }
            string label = tile.Label;
            if (string.IsNullOrWhiteSpace(label))
            {
                return;
            }
            int fontPx = Math.Max(8, (int)Math.Round(canvasHeight * 0.075));
            int margin = Math.Max(4, (int)Math.Round(canvasHeight * 0.09));
            Core.kt_image_add_label(Core.Utf8(path), Core.Utf8(label), fontPx, margin, 0xFFFFFF, Core.Utf8(path));
        }
        private static Rect FitCell(Rect cell, int canvasWidth, int canvasHeight)
        {
            if (canvasWidth <= 0 || canvasHeight <= 0)
            {
                return cell;
            }
            double aspect = canvasWidth / (double)canvasHeight;
            double width = Math.Min(cell.Width, cell.Height * aspect);
            double height = width / aspect;
            return new Rect(cell.X + ((cell.Width - width) / 2.0), cell.Y + ((cell.Height - height) / 2.0), width, height);
        }

        private static void CanvasSize(int size, out int width, out int height)
        {
            if (size == 0)
            {
                width = 70;
                height = 70;
                return;
            }
            if (size == 1)
            {
                width = 150;
                height = 150;
                return;
            }
            if (size == 2)
            {
                width = 310;
                height = 150;
                return;
            }
            width = 310;
            height = 310;
        }
    }
}
