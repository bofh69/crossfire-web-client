public struct SoundInfo {
    string file;
    uint vol;
}

/**
 * Load sound configuration file into a hash table of sound info structs.
 */
public HashTable? load_snd_config() {
    unowned string snd_dir = GLib.Environment.get_variable("CF_SOUND_DIR");
    var snd_config = @"$snd_dir/sounds.conf";
    string contents;
    try {
        FileUtils.get_contents(snd_config, out contents);
    } catch (FileError e) {
        stderr.printf("Could not read '%s'\n", snd_config);
        return null;
    }

    var sounds = new HashTable <string, SoundInfo?>(str_hash, str_equal);
    var lines = contents.split("\n");
    foreach (var line in lines) {
        if (line[0] == '#' || line.length == 0) {
            continue;
        }
        var entries = line.split(":");
        if (entries.length != 3) {
            stderr.printf("Parse error in sound configuration: '%s'\n", line);
            continue;
        }
        SoundInfo si = {entries[2], int.parse(entries[1])};
        sounds.insert(entries[0], si);
    }
    return sounds;
}
