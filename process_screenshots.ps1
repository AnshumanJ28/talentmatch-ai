Add-Type -AssemblyName System.Drawing
$inFolder = "c:\Users\aarrn\Desktop\Resume checker\talentmatch-ai-main\screenshots"
$outFolder = "c:\Users\aarrn\Desktop\Resume checker\talentmatch-ai-main\screenshots\processed"
if (-not (Test-Path $outFolder)) { New-Item -ItemType Directory -Path $outFolder | Out-Null }

$files = Get-ChildItem -Path $inFolder -File -Include *.png, *.jpg, *.jpeg -Recurse | Where-Object { $_.DirectoryName -ne $outFolder }

foreach ($f in $files) {
    try {
        $img = [System.Drawing.Image]::FromFile($f.FullName)
        
        # Target size
        $targetW = 1280
        $targetH = 800
        
        # Create blank 1280x800 image
        $bmp = New-Object System.Drawing.Bitmap $targetW, $targetH
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        
        # Background color (Dark Navy to match extension)
        $bgColor = [System.Drawing.Color]::FromArgb(255, 15, 23, 42)
        $brush = New-Object System.Drawing.SolidBrush $bgColor
        $g.FillRectangle($brush, 0, 0, $targetW, $targetH)
        
        # Calculate aspect ratio
        $ratioX = $targetW / $img.Width
        $ratioY = $targetH / $img.Height
        $ratio = [Math]::Min($ratioX, $ratioY)
        
        # If image is smaller than target, we could leave it original size or scale it up. Let's scale up slightly but limit ratio to 1
        # Actually, let's just use the ratio so it fits perfectly with padding
        $newW = [int]($img.Width * $ratio)
        $newH = [int]($img.Height * $ratio)
        
        $posX = [int](($targetW - $newW) / 2)
        $posY = [int](($targetH - $newH) / 2)
        
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($img, $posX, $posY, $newW, $newH)
        
        $outPath = Join-Path $outFolder ("formatted_" + $f.Name)
        $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
        
        $g.Dispose()
        $bmp.Dispose()
        $img.Dispose()
        $brush.Dispose()
        Write-Host "Processed $($f.Name)"
    } catch {
        Write-Host "Failed to process $($f.Name)"
    }
}
Write-Host "All done!"
