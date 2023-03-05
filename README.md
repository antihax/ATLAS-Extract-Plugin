[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/O5O33VK5S)

# ATLAS-Extract-Plugin
Plugin for ATLAS dedicated server to extract resources, bosses, and other imporant data.

## Usage 
*Do not run on a production server*. This can be ran on a Windows 10 PC.

1. install nodejs https://nodejs.org/en/
2. clone this repository
3. in a command window run `prereq.bat` to install the server, plugins, and configuration.
4. install your servergrid.json file to `DumpResources\server\ShooterGame`.
5. in a command window run `node build_json.js` and once all grids are processed the output will be in `./json`.
6. download [latest build of the site](https://github.com/antihax/atlasmap-js/releases/latest/download/dist.tar.gz)
7. copy the `json` directory to a the site directory
8. using the ServerGridEditor from Grapeshot Games, export the slippymap tiles to the site directory 
9. upload or host the site repository on a webserver

## Support
Please note I cannot offer support. If you manage to get it to work for you, that is awesome; this was not designed with unofficials clusters in mind.

## Commandline arguments
`nobuild` - Only process JSON and do not run servers. Useful for debugging.
`debug` - Print more information
`manualmanagedmods` - Don't use steamcmd to download mods.