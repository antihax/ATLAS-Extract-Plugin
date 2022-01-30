[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O5O33VK5S)

# ATLAS-Extract-Plugin
Plugin for ATLAS dedicated server to extract resources, bosses, and other imporant data.

## Usage 
*Do not run on a production server*. This can be ran on a Windows 10 PC.

- install nodejs https://nodejs.org/en/
- clone this repository
- in a command window run `prereq.bat` to install the server, plugins, and configuration.
- install your servergrid.json file to `DumpResources\server\ShooterGame`.
- in a command window run `node build_json.js` and once all grids are processed the output will be in `./json`.
- copy the contents of the `json` folder to a clone of https://github.com/antihax/antihax.github.io
- using the ServerGridEditor from Grapeshot Games, export the slippymap tiles and replace the `tiles` to the site repository
- upload or host the site repository on a webserver


## Support
Please note I cannot offer support. If you manage to get it to work for you, that is awesome; but this was not designed with unofficials clusters in mind.