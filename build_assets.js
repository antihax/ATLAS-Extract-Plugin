const fs = require('fs');
const execSync = require('child_process').execSync;
const helpers = require('./atlastools/include/helpers.js')

const baseDir = "DumpResources/server/ShooterGame/"
const resourceDir = baseDir + "Binaries/Win64/resources/"
const imgmgk = "DumpResources/imgmgk/"
const workDir = process.cwd();

let items = helpers.parseJSONFile(resourceDir + "items.json");

for (item in items) {
  let i = items[item];
  if (i.Icon) {
    console.log(i.Icon)
    execSync(`start /min ${imgmgk}convert "assets/${i.Icon}.TGA" -thumbnail 128 -format png -define png:compression-filter=5 -auto-orient -define png:compression-level=9 -quality 82 -define png:compression-strategy=1 "atlasicons/${i.Icon}_128.png"`)
    execSync(`start /min ${imgmgk}convert "assets/${i.Icon}.TGA" -thumbnail 64 -format png -define png:compression-filter=5 -auto-orient -define png:compression-level=9 -quality 82 -define png:compression-strategy=1 "atlasicons/${i.Icon}_64.png"`)
    execSync(`start /min ${imgmgk}convert "assets/${i.Icon}.TGA" -thumbnail 32 -format png -define png:compression-filter=5 -auto-orient -define png:compression-level=9 -quality 82 -define png:compression-strategy=1 "atlasicons/${i.Icon}_32.png"`)
  }
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