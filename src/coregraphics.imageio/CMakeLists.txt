if (APPLE)
    add_oiio_plugin (coregraphics.cpp
                     LINK_LIBRARIES "-framework ImageIO -framework CoreGraphics -framework CoreFoundation")
endif()
