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

function getFiles(dir) {
    return FS.readdir(dir)
        .filter(item => item !== '.' && item !== '..' && !item.startsWith('.'))
        .filter(item => FS.isFile(FS.stat(`${dir}/${item}`).mode))
        .filter(isRomFile);
}

function refreshRomFilesList() {
    const listContainer = document.querySelector("#download_rom_files");
    const ul = document.querySelector("#rom_list");

    ul.innerHTML = "";
    listContainer.style.display = "none";

    const rom_files = getFiles("gbemu_roms");

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

// Call after any change under /gbemu_roms, to persist it in IndexedDB.
function syncToStorage() {
    FS.syncfs(false, function (err) {
        if (err) {
            console.error("Error saving to persistent storage:", err);
        }
    });
}

// Initialize the ROM filesystem once the Emscripten runtime is ready.
Module.onRuntimeInitialized = function () {
    FS.mkdir("gbemu_roms");

    // Mount IDBFS for persistent storage, so uploaded ROMs survive a page
    // reload. IndexedDB is scoped per browser origin, not per page path, and
    // Emscripten's IDBFS uses the FS mount point itself as the IndexedDB
    // database name - a generic "/roms" here would collide with any other
    // Emscripten app mounting the same path on the same origin (e.g. one
    // hosted at a sibling GitHub Pages path), silently sharing/mixing their
    // persisted files. "/gbemu_roms" keeps this app's storage distinct.
    FS.mount(IDBFS, {}, "/gbemu_roms");

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
