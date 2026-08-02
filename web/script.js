function upload_rom_file_btn_click() {
    document.querySelector("#upload_rom_file").click()
}

function upload_rom_file() {
    const file = this.files[0];
    var reader = new FileReader();
    reader.readAsArrayBuffer(file);
    reader.onload = function (evt) {
        var buf = new Uint8Array(evt.target.result);
        FS.writeFile("roms/" + file.name, buf);
        refreshRomFilesList();
        syncToStorage();
    }
}

function loadRom(filename) {
    // Written as a one-shot trigger - App::checkEmscriptenLoadRequest()
    // (frontend.cpp) polls for this file once per frame, reads the
    // filename it names, then deletes it.
    const encoder = new TextEncoder();
    FS.writeFile("roms/.load_request", encoder.encode(filename));
}

function deleteRom(filename) {
    if (confirm(`Delete ROM file "${filename}"?`)) {
        try {
            FS.unlink(`roms/${filename}`);
            refreshRomFilesList();
            syncToStorage();
        } catch (e) {
            console.error("Failed to delete file:", e);
        }
    }
}

function getFiles(dir) {
    return FS.readdir(dir)
        .filter(item => item !== '.' && item !== '..' && !item.startsWith('.'))
        .filter(item => FS.isFile(FS.stat(`${dir}/${item}`).mode));
}

function refreshRomFilesList() {
    const listContainer = document.querySelector("#download_rom_files");
    const ul = document.querySelector("#rom_list");

    ul.innerHTML = "";
    listContainer.style.display = "none";

    const rom_files = getFiles("roms");

    if (rom_files.length === 0) {
        return;
    }

    rom_files.forEach(file => {
        const li = document.createElement("li");

        const playBtn = document.createElement("button");
        playBtn.className = "rom-action-btn play-btn";
        playBtn.title = "Load this ROM";
        playBtn.onclick = () => loadRom(file);

        const playIcon = document.createElement("img");
        playIcon.src = "img/play.svg";
        playBtn.appendChild(playIcon);

        const deleteBtn = document.createElement("button");
        deleteBtn.className = "rom-action-btn delete-btn";
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

// Call after any change under /roms, to persist it in IndexedDB.
function syncToStorage() {
    FS.syncfs(false, function (err) {
        if (err) {
            console.error("Error saving to persistent storage:", err);
        }
    });
}

// Initialize the ROM filesystem once the Emscripten runtime is ready.
Module.onRuntimeInitialized = function () {
    FS.mkdir("roms");

    // Mount IDBFS for persistent storage, so uploaded ROMs survive a
    // page reload.
    FS.mount(IDBFS, {}, "/roms");

    // Sync from IndexedDB to memory (populate=true means load from DB).
    FS.syncfs(true, function (err) {
        if (err) {
            console.error("Error loading persistent storage:", err);
        } else {
            console.log("Persistent storage loaded");
            refreshRomFilesList();
        }
    });

    document.querySelector("#upload_rom_file").addEventListener("change", upload_rom_file, false);
};

document.addEventListener("contextmenu", (e) => {
    if (e.target.tagName === "CANVAS") {
        e.preventDefault();
    }
});
