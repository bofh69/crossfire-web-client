public class SoundServer : Object {
    FileStream? pipe;

    public bool spawn(string server_path) {
        string[] args = {server_path};
        int pipe_in;
        try {
            Process.spawn_async_with_pipes(null, args, null, 0, null, null,
                                           out pipe_in);
            pipe = FileStream.fdopen(pipe_in, "w");
            return pipe != null;
        } catch (SpawnError e) {
            stderr.printf("Could not spawn sound server: %s\n", e.message);
            return false;
        }
    }

    /**
     * Initiates playing of a sound effect, specified by name and type, to
     * cfsndserv via a pipe.
     *
     * @param x      Offset of the sound relative to the player.
     * @param y      Offset of the sound relative to the player.
     * @param dir    The direction the sound is moving toward, where north = 1,
     *               northeast = 2, and so on.  0 indicates a stationary source.
     * @param vol    A value from 0 through 100 inclusive that suggests the
     *               relative loudness of the sound effect.
     * @param type   See server doc/Developers/sound for details.  1 is a sound
     *               related to living things.  2 is a spell-related sound.  3 is
     *               is made by an item.  4 is created by the environment.  5 is a
     *               sound of an attack.  6 is a sound of a incoming hit.  This is
     *               list may grow over time.
     * @param sound  A descriptive name for the sound effect to play.  It usually
     *               describes the sound itself, and may be combined with the type
     *               and source name to select the file to play.
     * @param source The name of the sound emitter.  It is used in combination
     *               with type and sound to determine which file to play.
     */
    public void play(int8 x, int8 y, uint8 dir, uint8 vol, uint8 type,
                     string sound, string source) {
        /*
         * NOTE: Sound and source are reversed with respect to how the server sent
         * data to the client.  This is intentional, so that the sound/music name
         * is always the last quoted string on the command sent to cfsndserv.
         */
        const string FORMAT = "%d %d %u %u %u \"%s\" \"%s\"\n";
        pipe.printf(FORMAT, x, y, dir, vol, type, source, sound);
        pipe.flush();
    }

    public void play_music(string music) {
        pipe.printf("\"%s\"\n", music);
        pipe.flush();
    }

    public void stop() {
        play_music("NONE");
    }
}
