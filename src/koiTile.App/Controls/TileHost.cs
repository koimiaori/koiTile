using System;
using System.Collections.Generic;
using Microsoft.UI.Xaml.Automation.Peers;
using Microsoft.UI.Xaml.Controls;

namespace koiTile.Controls
{
    public sealed class TileHost : ContentControl
    {
        public TileHost()
        {
            HorizontalContentAlignment = Microsoft.UI.Xaml.HorizontalAlignment.Stretch;
            VerticalContentAlignment = Microsoft.UI.Xaml.VerticalAlignment.Stretch;
        }

        protected override AutomationPeer OnCreateAutomationPeer()
        {
            return new FrameworkElementAutomationPeer(this);
        }
    }
}
