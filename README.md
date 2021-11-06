# UWPLayeredFS

Apply translations or mods to UWP apps without having to unpack/repack the app.

# How to use

1. Download latest release from [Release Page](https://github.com/44670/UWPLayeredFS/releases).
2. Extract the zip file, put you patched files into `layered` folder. (Just copy all the files, do not create or copy any folder.)
3. Open a Terminal and run the following command: `Launcher.exe [target-process-name]`. 

    (For example: `Launcher.exe notepad.exe`)

4. Now you can start you UWP app, and it will apply the translations or mods.
5. If you no longer need the LayeredFS, you can press any key in the terminal to exit.

# Limitations

1. Only amd64 is supported for now.
2. If the UWP app read resource files before creating the window, LayeredFS will not work.

    (This is because our code is activated as soon as the first window of UWP app is created.)
3. A DLL is loaded into the UWP app, and it intercepts the `NtCreateFile` function. Which may be not compatible with some anti-tampering software. **DO NOT use this on a Multi-Player game.**


