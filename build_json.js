const fs = require('fs');
const execSync = require('child_process').execSync;

const baseDir = "DumpResources/server/ShooterGame/"
const resourceDir = baseDir + "Binaries/Win64/resources/"
const workDir = process.cwd();

const serverConfig = parseJSONFile(baseDir + "ServerGrid.json");
const xGrids = serverConfig.totalGridsX;
const yGrids = serverConfig.totalGridsY;
const worldUnitsX = xGrids * serverConfig.gridSize;
const worldUnitsY = yGrids * serverConfig.gridSize;

if (!process.argv.includes("nobuild")) {
  process.chdir('DumpResources');
  for (let y = 0; y < yGrids; y++) {
    for (let x = 0; x < xGrids; x++) {
      execSync(process.cwd() + `\\server\\ShooterGame\\Binaries\\Win64\\ShooterGameServer.exe Ocean?ServerX=${x}?ServerY=${y}?AltSaveDirectoryName=${x}${y}?ServerAdminPassword=123?QueryPort=5001?Port=5002?MapPlayerLocation=true -NoBattlEye -log -server -NoSeamlessServer`)
    }
  }
  process.chdir(workDir);
}

// load resources
let grids = {};
let stones = [];
let bosses = [];
for (let x = 0; x < xGrids; x++) {
  for (let y = 0; y < yGrids; y++) {
    let grid = gridName(x, y);
    grids[grid] = parseJSONFile(resourceDir + grid + ".json");

    // save power stones
    if (grids[grid]["Stones"])
      for (stone in grids[grid]["Stones"])
        stones.push({
          name: stone,
          long: grids[grid]["Stones"][stone][0],
          lat: grids[grid]["Stones"][stone][1],
        });

    // save bosses
    if (grids[grid]["Boss"])
      for (boss in grids[grid]["Boss"])
        for (position in grids[grid]["Boss"][boss])
          bosses.push({
            name: boss,
            long: grids[grid]["Boss"][boss][position][0],
            lat: grids[grid]["Boss"][boss][position][1],
          });
  }
}
let animalcheck = new Set();
// match data to island instances
let islands = {};
let gridList = {};
let islandList = {};
for (let server in serverConfig.servers) {
  let s = serverConfig.servers[server];
  let grid = gridName(s.gridX, s.gridY);

  gridList[grid] = {};
  gridList[grid].name = s.name;
  gridList[grid].serverCustomDatas1 = s.ServerCustomDatas1;
  gridList[grid].serverCustomDatas2 = s.ServerCustomDatas2;
  gridList[grid].serverIslandPointsMultiplier = s.serverIslandPointsMultiplier;
  gridList[grid].biomes = new Set();


  for (let island in s.islandInstances) {
    let cp = s.islandInstances[island];
    let i = {
      id: cp.id,
      worldX: cp.worldX,
      worldY: cp.worldY,
      rotation: cp.rotation,
      name: cp.name,
      islandWidth: cp.islandWidth,
      islandHeight: cp.islandHeight,
      isControlPoint: cp.isControlPoint,
    };

    islands[i.id] = i;

    i.grid = grid;
    i.homeServer = s.isHomeServer ? true : false;
    i.resources = {};
    i.discoveries = [];

    gridBiomes = new Set();
    gridAnimals = new Set();
    gridResources = new Set();

    if (i.treasureMapSpawnPoints && i.treasureMapSpawnPoints.length > 0)
      i.resources["Treasure Spawns"] = i.treasureMapSpawnPoints.length;

    // build resource map
    for (let r in grids[grid].Resources) {
      if (inside(i, r.split(":")))
        for (let key in grids[grid].Resources[r]) {
          gridResources.add(key);
          if (!i.resources[key])
            i.resources[key] = grids[grid].Resources[r][key];
          else
            i.resources[key] += grids[grid].Resources[r][key];
        }
    }

    // build animal list
    let allanimals = new Set();
    for (let r in grids[grid].Animals) {
      if (inside(i, r.split(":")))
        for (let key in grids[grid].Animals[r]) {
          let animalList = grids[grid].Animals[r][key];
          if (process.argv.includes("debug"))
            if (
              animalList.levelOffset < 60 &&
              (animalList.levelLerp > 0.0 || animalList.levelOffset > 0 || animalList.levelMultiplier > 1.0 || animalList.islandLevelMultiplier > 1.0 || animalList.islandLevelOffset > 0 || animalList.levelMinOveride > 0))
              console.log(grid, animalList.levelLerp, animalList.levelOffset, animalList.levelMultiplier, animalList.islandLevelMultiplier, animalList.islandLevelOffset, animalList.levelMinOveride)


          for (let a in animalList.animals) {
            let animal = animalList.animals[a]
            gridAnimals.add(animal.name);
            allanimals.add(animal.name);
            animalcheck.add(animal.name);

            // print out special animals, check for changes
            if (process.argv.includes("debug"))
              if (animal.minLevelMultiplier || animal.maxLevelMultiplier || animal.minLevelOffset || animal.maxLevelOffset || animal.overrideLevel)
                console.log(animal)
          }
        }
    }
    i.animals = Array.from(allanimals).sort();

    for (let r in grids[grid].Maps) {
      if (inside(i, r.split(":")))
        i.maps = grids[grid].Maps[r];
    }

    for (let r in grids[grid].Meshes) {
      if (inside(i, r.split(":")))
        i.meshes = grids[grid].Meshes[r];
    }

    for (let r in s.discoZones) {
      let disco = s.discoZones[r];
      let d = grids[grid].Discoveries[disco.ManualVolumeName];
      if (d) {
        i.discoveries.push({
          name: disco.name,
          long: d[0],
          lat: d[1],
        });
      } else {
        if (insideNative(i, [disco.worldX, disco.worldY])) {
          let coords = worldToGPS(disco.worldX, disco.worldY);
          i.discoveries.push({
            name: disco.name,
            long: coords[0],
            lat: coords[1],
          });
        }
      }
    }

    let biomes = new Set();
    let biomeTags = new Set();
    for (let r in grids[grid].Biomes) {
      if (inside(i, r.split(":")))
        biomes.add(grids[grid].Biomes[r]);
      gridBiomes.add(grids[grid].Biomes[r].name);
    }
    i.biomes = Array.from(biomes).sort();
    i.biomeTags = Array.from(biomeTags).sort();
  }
  gridList[grid].biomes = Array.from(gridBiomes).sort();
  gridList[grid].animals = Array.from(gridAnimals).sort();
  gridList[grid].resources = Array.from(gridResources).sort();
}

if (process.argv.includes("debug"))
  for (let a in Array.from(animalcheck))
    console.log(Array.from(animalcheck)[a])


//stop();

// save everything
fs.writeFileSync('./json/config.js', "const config = " + JSON.stringify(sortObjByKey({
  ServersX: serverConfig.totalGridsX,
  ServersY: serverConfig.totalGridsY,
  GridSize: serverConfig.gridSize,
}), null, "\t"));

fs.writeFileSync('./json/stones.json', JSON.stringify(sortObjByKey(stones), null, "\t"));
fs.writeFileSync('./json/bosses.json', JSON.stringify(sortObjByKey(bosses), null, "\t"));
fs.writeFileSync('./json/islands.json', JSON.stringify(sortObjByKey(islands), null, "\t"));
fs.writeFileSync('./json/gridList.json', JSON.stringify(sortObjByKey(gridList), null, "\t"));
fs.writeFileSync('./json/shipPaths.json', JSON.stringify(sortObjByKey(serverConfig.shipPaths), null, "\t"));

function worldToGPS(x, y) {
  let long = ((x / worldUnitsX) * 200) - 100;
  let lat = 100 - ((y / worldUnitsY) * 200);
  return [long, lat];
}

function GPSToWorld(x, y) {
  let long = ((x + 100) / 200) * worldUnitsX;
  let lat = ((-y + 100) / 200) * worldUnitsY;
  return [long, lat];
}

function inside(i, c) {
  let coords = GPSToWorld(Number(c[0]), Number(c[1]));
  let
    x1 = i.worldX - (i.islandHeight / 2),
    y1 = i.worldY - (i.islandWidth / 2),
    x2 = i.worldX + (i.islandHeight / 2),
    y2 = i.worldY + (i.islandWidth / 2),
    x = Number(coords[0]),
    y = Number(coords[1]);

  if (x > x1 &&
    x < x2 &&
    y > y1 &&
    y < y2) {
    return true;
  }
  return false;
}

function insideNative(i, c) {

  let
    x1 = i.worldX - (i.islandHeight / 2),
    y1 = i.worldY - (i.islandWidth / 2),
    x2 = i.worldX + (i.islandHeight / 2),
    y2 = i.worldY + (i.islandWidth / 2),
    x = c[0],
    y = c[1];

  if (x > x1 &&
    x < x2 &&
    y > y1 &&
    y < y2) {
    return true;
  }
  return false;
}


function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function stop() {
  await sleep(2000000);
  // Sleep in loop
  for (let i = 0; i < 50; i++) {
    if (i === 3)
      await sleep(2000000);
  }
}

// helpers
function gridName(x, y) {
  return String.fromCharCode(65 + x) + (1 + y);
}

function parseJSONFile(file) {
  const rawdata = fs.readFileSync(file);
  return JSON.parse(rawdata);
}

function sortObjByKey(value) {
  return (typeof value === 'object') ?
    (Array.isArray(value) ?
      value.map(sortObjByKey) :
      Object.keys(value).sort().reduce(
        (o, key) => {
          const v = value[key];
          o[key] = sortObjByKey(v);
          return o;
        }, {})
    ) :
    value;
}