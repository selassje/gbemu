function upload_rom_file_btn_click() {
    document.querySelector("#upload_rom_file").click()
}

function isRomFile(filename) {
    return /\.(gb|gbc)$/i.test(filename);
}

function upload_rom_file() {
    const file = this.files[0];
    if (!isRomFile(file.name)) {
        alert("Only .gb/.gbc ROM files are supported.");
        return;
    }
    var reader = new FileReader();
    reader.readAsArrayBuffer(file);
    reader.onload = function (evt) {
        var buf = new Uint8Array(evt.target.result);
        FS.writeFile("gbemu_roms/" + file.name, buf);
        refreshRomFilesList();
        syncToStorage();
    }
}

function loadRom(filename) {
    // Written as a one-shot trigger - App::checkEmscriptenLoadRequest()
    // (frontend.cpp) polls for this file once per frame, reads the
    // filename it names, then deletes it.
    const encoder = new TextEncoder();
    FS.writeFile("gbemu_roms/.load_request", encoder.encode(filename));
    document.querySelector("#new_save_name").value = filename.replace(/\.(gb|gbc)$/i, "");
}

function deleteRom(filename) {
    if (confirm(`Delete ROM file "${filename}"?`)) {
        try {
            FS.unlink(`gbemu_roms/${filename}`);
            refreshRomFilesList();
            syncToStorage();
        } catch (e) {
            console.error("Failed to delete file:", e);
        }
    }
}

function upload_save_file_btn_click() {
    document.querySelector("#upload_save_file").click()
}

function isSaveFile(filename) {
    return /\.state$/i.test(filename);
}

function upload_save_file() {
    const file = this.files[0];
    if (!isSaveFile(file.name)) {
        alert("Only .state save files are supported.");
        return;
    }
    var reader = new FileReader();
    reader.readAsArrayBuffer(file);
    reader.onload = function (evt) {
        var buf = new Uint8Array(evt.target.result);
        FS.writeFile("gbemu_saves/" + file.name, buf);
        refreshSaveFilesList();
        syncToStorage();
    }
}

// Reads the name typed into #new_save_name (prefilled from the last ROM
// loaded - see loadRom()) and asks the emulator to write a new save state
// under it - saveState() itself is also reused by each existing save file's
// own "overwrite" button. Deliberately not a prompt()/confirm() dialog: both
// are blocking calls that would freeze Emscripten's main loop (and so the
// emulator itself, mid-game) for as long as the dialog stays open.
function new_save_state() {
    const name = document.querySelector("#new_save_name").value.trim();
    if (!name) {
        return;
    }
    saveState(isSaveFile(name) ? name : name + ".state");
}

function saveState(filename) {
    // Written as a one-shot trigger - App::checkEmscriptenSaveStateRequest()
    // (frontend.cpp) polls for this file once per frame, reads the filename
    // it names, then deletes it and writes saves/<filename> itself. Refresh
    // and sync are handled by the periodic timers in
    // Module.onRuntimeInitialized below, not here - this process hasn't
    // written saves/<filename> yet by the time this function returns.
    const encoder = new TextEncoder();
    FS.writeFile("gbemu_saves/.save_request", encoder.encode(filename));
}

function loadState(filename) {
    // Mirrors saveState() above - App::checkEmscriptenLoadStateRequest()
    // is the C++-side counterpart.
    const encoder = new TextEncoder();
    FS.writeFile("gbemu_saves/.load_request", encoder.encode(filename));
}

function deleteSaveState(filename) {
    if (confirm(`Delete save state "${filename}"?`)) {
        try {
            FS.unlink(`gbemu_saves/${filename}`);
            refreshSaveFilesList();
            syncToStorage();
        } catch (e) {
            console.error("Failed to delete file:", e);
        }
    }
}

function getFiles(dir, filterFn) {
    return FS.readdir(dir)
        .filter(item => item !== '.' && item !== '..' && !item.startsWith('.'))
        .filter(item => FS.isFile(FS.stat(`${dir}/${item}`).mode))
        .filter(filterFn);
}

function refreshRomFilesList() {
    const listContainer = document.querySelector("#download_rom_files");
    const ul = document.querySelector("#rom_list");

    ul.innerHTML = "";
    listContainer.style.display = "none";

    const rom_files = getFiles("gbemu_roms", isRomFile);

    if (rom_files.length === 0) {
        return;
    }

    rom_files.forEach(file => {
        const li = document.createElement("li");

        const playBtn = document.createElement("button");
        playBtn.className = "action-btn play-btn";
        playBtn.title = "Load this ROM";
        playBtn.onclick = () => loadRom(file);

        const playIcon = document.createElement("img");
        playIcon.src = "img/play.svg";
        playBtn.appendChild(playIcon);

        const deleteBtn = document.createElement("button");
        deleteBtn.className = "action-btn delete-btn";
        deleteBtn.title = "Delete this ROM";
        deleteBtn.onclick = () => deleteRom(file);

        const deleteIcon = document.createElement("img");
        deleteIcon.src = "img/delete.svg";
        deleteBtn.appendChild(deleteIcon);

        const span = document.createElement("span");
        span.textContent = file;

        li.appendChild(playBtn);
        li.appendChild(deleteBtn);
        li.appendChild(span);
        ul.appendChild(li);
    });

    listContainer.style.display = "block";
}

function refreshSaveFilesList() {
    const listContainer = document.querySelector("#download_save_files");
    const ul = document.querySelector("#save_list");

    ul.innerHTML = "";
    listContainer.style.display = "none";

    const save_files = getFiles("gbemu_saves", isSaveFile);

    if (save_files.length === 0) {
        return;
    }

    save_files.forEach(file => {
        const li = document.createElement("li");

        const playBtn = document.createElement("button");
        playBtn.className = "action-btn play-btn";
        playBtn.title = "Load this save state";
        playBtn.onclick = () => loadState(file);

        const playIcon = document.createElement("img");
        playIcon.src = "img/play.svg";
        playBtn.appendChild(playIcon);

        const saveBtn = document.createElement("button");
        saveBtn.className = "action-btn save-btn";
        saveBtn.title = "Overwrite this save state";
        saveBtn.onclick = () => saveState(file);

        const saveIcon = document.createElement("img");
        saveIcon.src = "img/save.svg";
        saveBtn.appendChild(saveIcon);

        const deleteBtn = document.createElement("button");
        deleteBtn.className = "action-btn delete-btn";
        deleteBtn.title = "Delete this save state";
        deleteBtn.onclick = () => deleteSaveState(file);

        const deleteIcon = document.createElement("img");
        deleteIcon.src = "img/delete.svg";
        deleteBtn.appendChild(deleteIcon);

        const span = document.createElement("span");
        span.textContent = file;

        li.appendChild(playBtn);
        li.appendChild(saveBtn);
        li.appendChild(deleteBtn);
        li.appendChild(span);
        ul.appendChild(li);
    });

    listContainer.style.display = "block";
}

// Call after any change under /gbemu_roms or /gbemu_saves, to persist it in
// IndexedDB.
function syncToStorage() {
    FS.syncfs(false, function (err) {
        if (err) {
            console.error("Error saving to persistent storage:", err);
        }
    });
}

// Initialize the ROM/save-state filesystems once the Emscripten runtime is
// ready.
Module.onRuntimeInitialized = function () {
    FS.mkdir("gbemu_roms");
    FS.mkdir("gbemu_saves");

    // Mount IDBFS for persistent storage, so uploaded ROMs and save states
    // survive a page reload. IndexedDB is scoped per browser origin, not per
    // page path, and Emscripten's IDBFS uses the FS mount point itself as
    // the IndexedDB database name - a generic "/roms"/"/saves" here would
    // collide with any other Emscripten app mounting the same path on the
    // same origin (e.g. one hosted at a sibling GitHub Pages path), silently
    // sharing/mixing their persisted files. The "gbemu_" prefix keeps this
    // app's storage distinct.
    FS.mount(IDBFS, {}, "/gbemu_roms");
    FS.mount(IDBFS, {}, "/gbemu_saves");

    // Sync from IndexedDB to memory (populate=true means load from DB).
    FS.syncfs(true, function (err) {
        if (err) {
            console.error("Error loading persistent storage:", err);
        } else {
            console.log("Persistent storage loaded");
            refreshRomFilesList();
            refreshSaveFilesList();
        }
    });

    // App::checkEmscriptenSaveStateRequest() (frontend.cpp) writes a new or
    // overwritten save file directly into /gbemu_saves on the C++ side, with
    // no JS call in the loop to trigger a list refresh or an IndexedDB sync
    // the way upload/delete above do - poll periodically to pick those up
    // instead.
    setInterval(() => {
        refreshSaveFilesList();
        syncToStorage();
    }, 5000);

    document.querySelector("#upload_rom_file").addEventListener("change", upload_rom_file, false);
    document.querySelector("#upload_save_file").addEventListener("change", upload_save_file, false);
};

document.addEventListener("contextmenu", (e) => {
    if (e.target.tagName === "CANVAS") {
        e.preventDefault();
    }
});

document.querySelector("#new_save_name").addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
        new_save_state();
    }
});
