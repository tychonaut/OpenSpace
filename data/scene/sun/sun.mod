return {
    -- Sun barycenter module
    {
        Name = "SolarSystemBarycenter",
        Parent = "SolarSystem",
        -- SceneRadius unit is KM                
		SceneRadius = 6000000000.0,
        Ephemeris = {
            Type = "Static",
        },
    },

    -- Sun module
    {   
        Name = "Sun",
        Parent = "SolarSystemBarycenter",
         -- SceneRadius unit is KM                
		SceneRadius = 30000000.0,
        Renderable = {
            Type = "RenderablePlanet",
            Frame = "IAU_SUN",
            Body = "SUN", 
            Geometry = {
                Type = "SimpleSphere",
                Radius = { 2.783200, 9 },
                Segments = 100
            },
            Textures = {
                Type = "simple",
                Color = "textures/sun.jpg",
            },
            PerformShading = false,
        },
        Ephemeris = {
            Type = "Spice",
            Body = "SUN",
            Reference = "GALACTIC",
            Observer = "SSB",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },
        Rotation = {
            Type = "Spice",
            Frame = "IAU_SUN",
            Reference = "GALACTIC"
        },
    },
    {
        Name = "SunGlare",
        Parent = "SolarSystemBarycenter",
        Renderable = {
            Type = "RenderablePlane",
            Size = {1.3, 10.5},
            Origin = "Center",
            Billboard = true,
            Texture = "textures/sun-glare.png"
        },
        Ephemeris = {
            Type = "Spice",
            Body = "SUN",
            Reference = "GALACTIC",
            Observer = "SSB",
            Kernels = {
                "${OPENSPACE_DATA}/spice/de430_1850-2150.bsp"
            }
        },        
    },
    {
        Name = "SunMarker",
        Parent = "Sun",
        Renderable = {
            Type = "RenderablePlane",
            Size = {3.0, 11.0},
            Origin = "Center",
            Billboard = true,
            Texture = "textures/marker.png"
        },
        Ephemeris = {
            Type = "Static",
            Position = {0, 0, 0, 5}
        }
    }
}
