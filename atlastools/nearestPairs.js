// Check for broken or missing nodes

'use strict';
const helpers = require('./include/helpers.js');

const baseDir = 'DumpResources/server/ShooterGame/';
const resourceDir = baseDir + 'Binaries/Win64/resources/';
const workDir = process.cwd();

const serverConfig = helpers.parseJSONFile(baseDir + 'ServerGrid.json');
const xGrids = serverConfig.totalGridsX;
const yGrids = serverConfig.totalGridsY;
const worldUnitsX = xGrids * serverConfig.gridSize;
const worldUnitsY = yGrids * serverConfig.gridSize;
for (let x = 0; x < xGrids; x++) {
	for (let y = 0; y < yGrids; y++) {
		let grid = helpers.gridName(x, y);

		// Math.hypot();
	}
}
