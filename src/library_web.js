// src/library_web.js
mergeInto(LibraryManager.library, {
  // The function name in JS must start with an underscore.
  // Emscripten will link this to the C function `downloadFile` (without the underscore).
  _downloadFile: function(dataPtr, dataSize, filenamePtr) {
    // Convert the C pointers to JavaScript data
    const filename = UTF8ToString(filenamePtr);
    const data = HEAPU8.slice(dataPtr, dataPtr + dataSize);

    // Create a Blob from the data
    const blob = new Blob([data], { type: "application/octet-stream" });

    // Create a temporary link element to trigger the download
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = filename;

    // Simulate a click to start the download
    document.body.appendChild(link);
    link.click();

    // Clean up the temporary link and URL
    document.body.removeChild(link);
    URL.revokeObjectURL(link.href);
  },
});