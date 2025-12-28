{
    "Source": "UIElement.azsl",
    "DepthStencilState": {
        "Depth": {
            "Enable": false
        }
    },
    "RasterState": {
        "CullMode": "None",
        "FillMode": "Solid",
        "depthClipEnable": false
    },
    "GlobalTargetBlendState":
    {
        "Enable": true,
        "BlendSource": "One",
        "BlendDest": "AlphaSourceInverse",
        "BlendOp": "Add",
        "BlendAlphaSource": "One",
        "BlendAlphaDest": "AlphaSourceInverse",
        "BlendAlphaOp": "Add"
    },
    "ProgramSettings": {
        "EntryPoints": [
            {
                "name": "MainVS",
                "type": "Vertex"
            },
            {
                "name": "MainPS",
                "type": "Fragment"
            }
        ]
    }
}
