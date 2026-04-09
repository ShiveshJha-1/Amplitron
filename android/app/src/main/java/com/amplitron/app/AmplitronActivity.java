package com.amplitron.app;

import org.libsdl.app.SDLActivity;

/**
 * AmplitronActivity — SDL2 entry point for the Amplitron guitar amp simulator.
 *
 * SDL2 ships libmain.so which already embeds libSDL2 statically, so we
 * override getLibraries() to load only "main" instead of the default
 * ["SDL2", "main"] pair that SDLActivity would otherwise request.
 */
public class AmplitronActivity extends SDLActivity {

    /**
     * Returns the list of shared libraries to load before calling into C++.
     * Because SDL2 is linked statically into libmain.so there is no
     * separate libSDL2.so to load.
     */
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    /**
     * Application name displayed in the title bar / task switcher.
     */
    @Override
    protected String getMainSharedObject() {
        return "libmain.so";
    }
}
