{
    "Source" : "ClearStencil.azsl",
    "DepthStencilState" : 
    {
        "Depth" : { "Enable" : false },
        "Stencil" : { "Enable" : true }
    },
    "GlobalTargetBlendState" : {
        "Enable" : false
    },
    "RasterState" : {
        "CullMode" : "None"
    },
    "ProgramSettings":
    {
      "EntryPoints":
      [
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