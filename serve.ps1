$port = 8080
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$port/")
$listener.Start()
Write-Host "HTTP server running at http://localhost:$port/"

$root = $PSScriptRoot

while ($listener.IsListening) {
    $context = $listener.GetContext()
    $request = $context.Request
    $response = $context.Response

    $path = $request.Url.LocalPath
    if ($path -eq "/" -or $path -eq "") {
        $path = "/college_slides.html"
    }

    $filePath = Join-Path $root ($path.TrimStart('/'))

    if (Test-Path $filePath -PathType Leaf) {
        $bytes = [System.IO.File]::ReadAllBytes($filePath)
        $response.AddHeader("Cache-Control", "no-cache, no-store, must-revalidate")
        $response.AddHeader("Pragma", "no-cache")
        $response.AddHeader("Expires", "0")
        if ($filePath.EndsWith(".html")) {
            $response.ContentType = "text/html; charset=utf-8"
        } elseif ($filePath.EndsWith(".png")) {
            $response.ContentType = "image/png"
        } elseif ($filePath.EndsWith(".css")) {
            $response.ContentType = "text/css"
        } elseif ($filePath.EndsWith(".js")) {
            $response.ContentType = "application/javascript"
        } else {
            $response.ContentType = "application/octet-stream"
        }
        $response.ContentLength64 = $bytes.Length
        $response.OutputStream.Write($bytes, 0, $bytes.Length)
    } else {
        $response.StatusCode = 404
        $msg = [System.Text.Encoding]::UTF8.GetBytes("404 Not Found")
        $response.OutputStream.Write($msg, 0, $msg.Length)
    }
    $response.OutputStream.Close()
}
