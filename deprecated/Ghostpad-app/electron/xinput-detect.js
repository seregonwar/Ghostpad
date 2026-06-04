/**
 * XInput controller enumeration via PowerShell + xinput1_4.dll.
 * Bypasses the Web Gamepad API's "button press required" activation model.
 * Returns an array of { index: number, name: string } for connected controllers.
 * Windows only; resolves to [] on other platforms.
 */

const { execFile } = require("child_process");

// UTF-16LE base64 encoding avoids all quote/escape issues when passing
// complex scripts to powershell -EncodedCommand.
const SCRIPT = `
$ErrorActionPreference = 'SilentlyContinue'
$src = @'
using System.Runtime.InteropServices;
public class GhostpadXInput {
    [DllImport("xinput1_4.dll", SetLastError = false)]
    public static extern int XInputGetState(int dwUserIndex, byte[] pState);
}
'@
Add-Type -TypeDefinition $src -ErrorAction SilentlyContinue
$r = @()
for ($i = 0; $i -lt 4; $i++) {
    $s = New-Object byte[] 16
    if ([GhostpadXInput]::XInputGetState($i, $s) -eq 0) {
        $r += [pscustomobject]@{ index = $i; name = "XInput Controller $($i + 1)" }
    }
}
if ($r.Count -eq 0) { Write-Output '[]' } else { $r | ConvertTo-Json -AsArray -Compress }
`.trim();

const ENCODED = Buffer.from(SCRIPT, "utf16le").toString("base64");

/**
 * @returns {Promise<Array<{index: number, name: string}>>}
 */
function listXInputControllers() {
  if (process.platform !== "win32") return Promise.resolve([]);

  return new Promise((resolve) => {
    execFile(
      "powershell",
      [
        "-NoProfile",
        "-NonInteractive",
        "-WindowStyle", "Hidden",
        "-EncodedCommand", ENCODED,
      ],
      { timeout: 8000, encoding: "utf8", windowsHide: true },
      (err, stdout) => {
        if (err) {
          resolve([]);
          return;
        }
        try {
          const parsed = JSON.parse(stdout.trim() || "[]");
          resolve(Array.isArray(parsed) ? parsed : []);
        } catch {
          resolve([]);
        }
      }
    );
  });
}

module.exports = { listXInputControllers };
