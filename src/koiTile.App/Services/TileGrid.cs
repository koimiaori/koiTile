using System;
using System.Collections.Generic;
using koiTile.Models;
using Windows.Foundation;

namespace koiTile.Services
{
    internal static class TileGrid
    {
        public const int Unit = 48;
        public const int Gap = 4;
        public const int Step = Unit + Gap;
        public const int Margin = 38;
        public const int SmallBox = 48;
        public const int MaxSpan = 4;
        public const int GroupSize = 8;
        public const int GroupGap = 16;
        public const int GroupStep = GroupSize * Step + GroupGap - Gap;
        public const int MaxGroups = 8;
        public const int MaxColumns = GroupSize * MaxGroups;
        public const int MinSize = 0;
        public const int MaxSize = 3;

        public static int SizeCols(int size)
        {
            if (size == 2)
            {
                return 4;
            }
            if (size == 3)
            {
                return 4;
            }
            return size == 1 ? 2 : 1;
        }

        public static int SizeRows(int size)
        {
            if (size == 3)
            {
                return 4;
            }
            return size == 1 || size == 2 ? 2 : 1;
        }

        public static int SpanPixels(int span)
        {
            return span * Unit + (span - 1) * Gap;
        }

        public static int ColumnLeft(int column)
        {
            return column / GroupSize * GroupStep + column % GroupSize * Step;
        }

        public static int ExtentWidth(int columns)
        {
            if (columns < 1)
            {
                columns = 1;
            }
            return ColumnLeft(columns - 1) + Unit;
        }

        public static int UnitsAt(double pixels, int limit)
        {
            int column;
            if (pixels <= 0)
            {
                column = 1;
            }
            else
            {
                int group = (int)(pixels / GroupStep);
                double inside = pixels - group * (double)GroupStep;
                column = group * GroupSize + (int)Math.Round(inside / Step);
            }
            if (column < 1)
            {
                column = 1;
            }
            if (column > limit)
            {
                column = limit;
            }
            return column;
        }

        public static bool InGroup(int column, int span)
        {
            return column >= 0 && column / GroupSize == (column + span - 1) / GroupSize;
        }

        public static int GroupStart(int column)
        {
            return column / GroupSize * GroupSize;
        }

        public static int Columns(IList<TileConfig> tiles)
        {
            int columns = 1;
            foreach (TileConfig tile in tiles)
            {
                int span = SizeCols(tile.Size);
                if (tile.GridCol + span > columns)
                {
                    columns = tile.GridCol + span;
                }
            }
            return columns;
        }

        public static int Rows(IList<TileConfig> tiles)
        {
            int rows = 1;
            foreach (TileConfig tile in tiles)
            {
                int span = SizeRows(tile.Size);
                if (tile.GridRow + span > rows)
                {
                    rows = tile.GridRow + span;
                }
            }
            return rows;
        }

        public static int Width(IList<TileConfig> tiles)
        {
            return ExtentWidth(Columns(tiles));
        }

        public static int Height(IList<TileConfig> tiles)
        {
            return SpanPixels(Rows(tiles));
        }

        public static Rect RectOf(TileConfig tile)
        {
            return new Rect(
                ColumnLeft(tile.GridCol),
                tile.GridRow * Step,
                SpanPixels(SizeCols(tile.Size)),
                SpanPixels(SizeRows(tile.Size)));
        }

        public static Rect BoxOf(TileConfig tile)
        {
            return RectOf(tile);
        }

        public static bool IsFree(IList<TileConfig> tiles, TileConfig self, int column, int row, int columns, int rows)
        {
            foreach (TileConfig tile in tiles)
            {
                if (ReferenceEquals(tile, self))
                {
                    continue;
                }
                if (!Overlaps(tile, column, row, columns, rows))
                {
                    continue;
                }
                return false;
            }
            return true;
        }

        private static bool Overlaps(TileConfig tile, int column, int row, int columns, int rows)
        {
            int left = tile.GridCol;
            int top = tile.GridRow;
            int right = left + SizeCols(tile.Size);
            int bottom = top + SizeRows(tile.Size);
            bool apart = column + columns <= left || column >= right || row + rows <= top || row >= bottom;
            return !apart;
        }

        private static bool Inside(int column, int row, int left, int top, int right, int bottom)
        {
            return column >= left && column < right && row >= top && row < bottom;
        }

        private static bool Open(int column, int row, int span, int left, int top, int right, int bottom, int skipLeft, int skipTop, int skipRight, int skipBottom)
        {
            if (!InGroup(column, span))
            {
                return false;
            }
            for (int r = 0; r < span; r++)
            {
                for (int c = 0; c < span; c++)
                {
                    if (!Inside(column + c, row + r, left, top, right, bottom))
                    {
                        return false;
                    }
                    if (Inside(column + c, row + r, skipLeft, skipTop, skipRight, skipBottom))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        public static List<TileConfig> Fill(IList<TileConfig> tiles, int left, int top, int right, int bottom, int skipLeft, int skipTop, int skipRight, int skipBottom, Func<string> nextId)
        {
            var created = new List<TileConfig>();
            for (int pass = 0; pass < 3; pass++)
            {
                int span = pass == 2 ? 1 : 2;
                for (int row = top; row < bottom; row++)
                {
                    for (int column = left; column < right; column++)
                    {
                        if (pass == 0 && (column % 2 != 0 || row % 2 != 0))
                        {
                            continue;
                        }
                        if (!Open(column, row, span, left, top, right, bottom, skipLeft, skipTop, skipRight, skipBottom))
                        {
                            continue;
                        }
                        if (!IsFree(tiles, null, column, row, span, span))
                        {
                            continue;
                        }
                        var tile = new TileConfig
                        {
                            Id = nextId(),
                            Size = span == 2 ? 1 : 0,
                            GridCol = column,
                            GridRow = row
                        };
                        tiles.Add(tile);
                        created.Add(tile);
                    }
                }
            }
            return created;
        }

        public static bool SetSize(IList<TileConfig> tiles, TileConfig tile, int size, Func<string> nextId)
        {
            if (tile == null || size < MinSize || size > MaxSize)
            {
                return false;
            }
            if (tile.Size == size)
            {
                return false;
            }
            int left = tile.GridCol;
            int top = tile.GridRow;
            int right = left + SizeCols(tile.Size);
            int bottom = top + SizeRows(tile.Size);
            int columns = SizeCols(size);
            int rows = SizeRows(size);
            int column = left;
            int groupStart = GroupStart(left);
            if (column + columns > groupStart + GroupSize)
            {
                column = Math.Max(groupStart, groupStart + GroupSize - columns);
            }
            var freed = new List<(int Left, int Top, int Right, int Bottom)>();
            for (int i = tiles.Count - 1; i >= 0; i--)
            {
                TileConfig other = tiles[i];
                if (ReferenceEquals(other, tile) || !Overlaps(other, column, top, columns, rows))
                {
                    continue;
                }
                freed.Add((other.GridCol, other.GridRow, other.GridCol + SizeCols(other.Size), other.GridRow + SizeRows(other.Size)));
                tiles.RemoveAt(i);
            }
            tile.GridCol = column;
            tile.Size = size;
            int fillColumns = Math.Min(MaxColumns, Math.Max(Columns(tiles), Math.Max(right, column + columns)));
            int fillRows = Math.Max(Rows(tiles), Math.Max(bottom, top + rows));
            foreach ((int Left, int Top, int Right, int Bottom) rect in freed)
            {
                fillColumns = Math.Min(MaxColumns, Math.Max(fillColumns, rect.Right));
                fillRows = Math.Max(fillRows, rect.Bottom);
            }
            Fill(tiles, 0, 0, fillColumns, fillRows, 0, 0, 0, 0, nextId);
            return true;
        }

        public static void Clamp(IList<TileConfig> tiles)
        {
            foreach (TileConfig tile in tiles)
            {
                int span = SizeCols(tile.Size);
                int groupStart = GroupStart(tile.GridCol);
                if (tile.GridCol + span > groupStart + GroupSize)
                {
                    tile.GridCol = Math.Max(groupStart, groupStart + GroupSize - span);
                }
            }
            for (int i = tiles.Count - 1; i >= 0; i--)
            {
                TileConfig tile = tiles[i];
                if (!IsFree(tiles, tile, tile.GridCol, tile.GridRow, SizeCols(tile.Size), SizeRows(tile.Size)))
                {
                    tiles.RemoveAt(i);
                }
            }
        }

        public static void Initialize(IList<TileConfig> tiles, Func<string> nextId)
        {
            if (tiles.Count > 0)
            {
                return;
            }
            tiles.Add(new TileConfig
            {
                Id = nextId(),
                Size = 1,
                ShowName = false,
                GridCol = 0,
                GridRow = 0
            });
        }
    }
}
