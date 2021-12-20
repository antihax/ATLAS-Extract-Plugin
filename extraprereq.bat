mkdir DumpResources\imgmgk
mkdir atlasicons
powershell -Command "(New-Object Net.WebClient).DownloadFile('https://download.imagemagick.org/ImageMagick/download/binaries/ImageMagick-7.1.0-portable-Q16-x64.zip', 'DumpResources/imgmgk/imgmgk.zip')"
tar -xf DumpResources/imgmgk/imgmgk.zip -C DumpResources/imgmgk