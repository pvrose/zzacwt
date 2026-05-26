# Convert rose.png from red/pink to apple color
# This script transforms red/pink hues to apple tones while preserving transparency

Add-Type -AssemblyName System.Drawing

$inputFile = "rose_original_backup.png"
$outputFile = "rose.png"
$tempFile = "rose_temp.png"
$backupFile = "rose_original_backup.png"

Write-Host "Converting rose colors from red/pink to apple..."
Write-Host "Original backed up to: $backupFile"

# Load the image
$originalImage = [System.Drawing.Bitmap]::new($inputFile)
$width = $originalImage.Width
$height = $originalImage.Height

Write-Host "Image size: ${width}x${height}"

# Create new bitmap for the result
$newImage = [System.Drawing.Bitmap]::new($width, $height)

# apple color target (adjust these values for different apple tones)
$appleR = 191
$appleG = 255
$appleB = 191

# Process each pixel
$pixelsProcessed = 0
for ($y = 0; $y -lt $height; $y++) {
    for ($x = 0; $x -lt $width; $x++) {
        $pixel = $originalImage.GetPixel($x, $y)

        # Keep fully transparent pixels as-is
        if ($pixel.A -eq 0) {
            $newImage.SetPixel($x, $y, $pixel)
            continue
        }

        # Calculate perceived brightness and saturation
        $maxRGB = [Math]::Max([Math]::Max($pixel.R, $pixel.G), $pixel.B)
        $minRGB = [Math]::Min([Math]::Min($pixel.R, $pixel.G), $pixel.B)
        $brightness = ($maxRGB + $minRGB) / 2.0

        # Calculate saturation
        if ($maxRGB -eq $minRGB) {
            $saturation = 0
        } else {
            if ($brightness -lt 128) {
                $saturation = ($maxRGB - $minRGB) / ($maxRGB + $minRGB)
            } else {
                $saturation = ($maxRGB - $minRGB) / (510 - $maxRGB - $minRGB)
            }
        }

        # Check if this is a red/pink/magenta pixel (high red component)
        # We want to convert red-ish colors to apple
        $isReddish = $pixel.R -gt $pixel.G -and $pixel.R -gt $pixel.B

        if ($isReddish) {
            # This is a red/pink pixel - convert to apple
            # Maintain the same brightness/value but shift hue to apple

            # Calculate brightness ratio
            $brightnessRatio = $brightness / 128.0

            # Apply apple color with brightness adjustment
            $newR = [Math]::Min(255, [int]($appleR * $brightnessRatio))
            $newG = [Math]::Min(255, [int]($appleG * $brightnessRatio))
            $newB = [Math]::Min(255, [int]($appleB * $brightnessRatio))

            # Preserve alpha channel
            $newPixel = [System.Drawing.Color]::FromArgb($pixel.A, $newR, $newG, $newB)
            $newImage.SetPixel($x, $y, $newPixel)
            $pixelsProcessed++
        } else {
            # Not a red pixel - keep original (might be shadows, stems, etc.)
            $newImage.SetPixel($x, $y, $pixel)
        }
    }

    # Progress indicator
    if (($y % 10) -eq 0) {
        $progress = [int](($y / $height) * 100)
        Write-Progress -Activity "Converting colors" -Status "$progress% Complete" -PercentComplete $progress
    }
}

Write-Progress -Activity "Converting colors" -Completed

Write-Host "Processed $pixelsProcessed red/pink pixels"
Write-Host "Saving converted image..."

# Save to temp file first
$newImage.Save($tempFile, [System.Drawing.Imaging.ImageFormat]::Png)

# Cleanup
$originalImage.Dispose()
$newImage.Dispose()

# Now move temp file to final location
if (Test-Path $outputFile) {
    Remove-Item $outputFile -Force
}
Move-Item $tempFile $outputFile -Force

Write-Host "Done! Rose is now apple-colored."
Write-Host "Original saved as: $backupFile"
