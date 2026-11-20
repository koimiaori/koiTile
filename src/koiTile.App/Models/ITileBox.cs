namespace koiTile.Models
{
    internal interface ITileBox
    {
        int Size { get; set; }

        int GridCol { get; set; }

        int GridRow { get; set; }
    }
}
