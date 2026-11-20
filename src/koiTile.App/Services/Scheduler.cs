using System;
using System.Collections.Generic;
using koiTile.Interop;
using koiTile.Models;
using Microsoft.UI.Dispatching;

namespace koiTile.Services
{
    internal sealed class Scheduler
    {
        public static readonly Scheduler Instance = new Scheduler();

        private DispatcherQueueTimer _timer;

        public bool Running { get; private set; }

        public void Start(DispatcherQueue queue)
        {
            Core.kt_schedule_reset();
            Core.kt_schedule_sync(Workspace.Instance.Config);
            if (_timer == null)
            {
                _timer = queue.CreateTimer();
                _timer.Interval = TimeSpan.FromSeconds(1);
                _timer.Tick += OnTick;
            }
            _timer.Start();
            Running = true;
        }

        public void Stop()
        {
            if (_timer != null)
            {
                _timer.Stop();
            }
            Running = false;
        }

        public void Sync()
        {
            Core.kt_schedule_reset();
            Core.kt_schedule_sync(Workspace.Instance.Config);
        }

        public void MarkNow(TileConfig tile)
        {
            if (tile == null)
            {
                return;
            }
            Core.kt_schedule_mark(Workspace.Instance.Config, Core.Utf8(tile.Id));
        }

        public string RefreshNow(TileConfig tile)
        {
            if (tile == null)
            {
                return string.Empty;
            }
            string text = Fetch(tile);
            TileNotifier.Push(tile, text, Workspace.Instance.PeekImage(tile), true);
            return text;
        }

        private void OnTick(DispatcherQueueTimer sender, object args)
        {
            if (!Running)
            {
                return;
            }

            try
            {
                var config = Workspace.Instance.Config;
                byte[] buffer = new byte[8192];
                int count = 0;
                if (Core.kt_schedule_due(config, buffer, buffer.Length, ref count) == 0 && count > 0)
                {
                    foreach (string id in Split(Core.Decode(buffer)))
                    {
                        TileConfig tile = Find(id);
                        if (tile == null)
                        {
                            continue;
                        }
                        TileNotifier.Push(tile, Fetch(tile), Workspace.Instance.PeekImage(tile));
                    }
                }

                int peekCount = 0;
                byte[] peekBuffer = new byte[8192];
                if (Core.kt_schedule_due_peek(config, peekBuffer, peekBuffer.Length, ref peekCount) == 0 && peekCount > 0)
                {
                    foreach (string id in Split(Core.Decode(peekBuffer)))
                    {
                        TileConfig tile = Find(id);
                        if (tile == null)
                        {
                            continue;
                        }
                        Workspace.Instance.AdvancePeek(tile);
                        TileNotifier.Push(tile, Fetch(tile), Workspace.Instance.PeekImage(tile));
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Write("tick failed: " + ex.Message);
            }
        }

        private static string Fetch(TileConfig tile)
        {
            return Core.Get((buffer, length) => Core.kt_text_fetch(Workspace.Instance.Config, Core.Utf8(tile.Id), buffer, length), 8192);
        }

        private static TileConfig Find(string id)
        {
            foreach (TileConfig tile in Workspace.Instance.Tiles)
            {
                if (tile.Id == id)
                {
                    return tile;
                }
            }
            return null;
        }

        private static IEnumerable<string> Split(string value)
        {
            foreach (string part in value.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries))
            {
                string item = part.Trim();
                if (item.Length > 0)
                {
                    yield return item;
                }
            }
        }
    }
}
