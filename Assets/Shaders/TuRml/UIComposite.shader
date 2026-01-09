{
    "Source": "UIComposite.azsl",
    "DepthStencilState": {
        "Depth": {
            "Enable": false
        }
    },
    "GlobalTargetBlendState":
    {
        "Enable": true,
        "BlendSource": "One",
        "BlendDest": "AlphaSourceInverse",
        "BlendOp": "Add"
    },
    "DrawList": "forward",
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