# Computer Repair Center — Build & Run

This build uses the uploaded `repair_center.c` as its source of truth. The HTML is a single self-contained frontend; the C executable owns ticket/inventory operations and reads/writes the original binary `tickets.dat` and `parts.dat` records.

## Files

- `Computer_Repair_Center.html` — open directly in a modern browser.
- `repair_center_backend.c` — C source for the local Winsock API.

## Build in Dev-C++ (Windows)

1. Keep `repair_center_backend.c` in the folder where you want the data files (`tickets.dat` and `parts.dat`) to live. Existing files should be placed in this same working folder; their record layouts are preserved.
2. Open Dev-C++ and create/open a C project, then add `repair_center_backend.c` as the project source. Use a MinGW-w64 GCC toolchain with C99 support.
3. In **Project → Project Options → Parameters → Linker**, add:

   ```text
   -lws2_32
   ```

   (If compiling from a GCC command prompt instead, the equivalent is `gcc -std=c99 repair_center_backend.c -o repair_center_backend.exe -lws2_32`.)
4. Build/compile. The Winsock library option is required for the Windows networking calls.

## Run

1. Run `repair_center_backend.exe` from its data folder. Keep its console window open. It listens only on `127.0.0.1:8765`; it does not bind to a public network interface. The same C process serves the single HTML file at `http://127.0.0.1:8765/` as well as the API.
2. Either open `http://127.0.0.1:8765/` in Edge, Chrome, Firefox, or another modern browser, or open `Computer_Repair_Center.html` directly. Choose the Service Desk or Administrator portal. If the backend starts after the page, press **Retry connection** or refresh the page.
3. To stop the backend, close its console window or press **Ctrl+C**.

The server processes requests sequentially, refreshes records from the data files on requests, and writes changed records back using the original `Ticket` and `Part` binary struct layouts. Back up data before using the system with valuable records.

## Administrator demonstration login

- Username: `admin567`
- Password: `admin123`

The C backend checks these credentials. The password is not embedded in the HTML or returned by the API. A server-side session token protects administrator endpoints and is invalidated on logout.

## API and safety notes

The local API is `http://127.0.0.1:8765`. Its CORS handling supports the `null` origin used by a locally opened HTML file; non-local browser origins are rejected. Ticket IDs follow the original `101 + record count` convention and skip any existing duplicate ID. The C arrays remain capped at 100 records, matching the original project.

This is a local demonstration, not a production security boundary. The demo password is hardcoded in the C source, the loopback HTTP service is unencrypted, and local programs running as the same user may inspect or call it. Do not expose/forward port 8765 or use these credentials for a real service. The source includes an `ARENA_PREVIEW_MODE` build switch for isolated hosted previews; it binds to all interfaces and relaxes origin checks only when explicitly compiled with that macro. Do not use that preview build on your own network or as the Dev-C++ executable; the normal build remains loopback-only. The backup action downloads the original binary data-file snapshots; preserve both files together with the same compiler/record layout.
