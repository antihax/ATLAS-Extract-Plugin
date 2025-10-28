const fs = require('fs');
const zlib = require('zlib');
const path = require('path');
const execSync = require('child_process').execSync;
const {promisify} = require('util');
const {unlink} = require('fs').promises;

const unzip = promisify(zlib.unzip);
const modFolder = 'DumpResources/server/ShooterGame/Content/Mods/';

class UnpackReader {
  constructor(buffer) {
    this.buffer = buffer;
    this.offset = 0;
  }

  readBigInt64LE() {
    const value = this.buffer.readBigInt64LE(this.offset);
    this.offset += 8;
    return value;
  }

  readInt32LE() {
    const value = this.buffer.readInt32LE(this.offset);
    this.offset += 4;
    return value;
  }

  getOffset() {
    return this.offset;
  }

  readChunk(size) {
    const chunk = this.buffer.slice(this.offset, this.offset + size);
    this.offset += size;
    return chunk;
  }
}

async function unpackZFilesRecursive(dir) {
  const files = fs.readdirSync(dir);
  const tasks = [];

  for (const file of files) {
    const fullPath = path.join(dir, file);
    const stat = fs.statSync(fullPath);

    if (stat.isDirectory()) {
      tasks.push(unpackZFilesRecursive(fullPath));
    } else if (file.endsWith('.z')) {
      const dst = fullPath.slice(0, -2);
      tasks.push(
          unpack(fullPath, dst).then(() => deleteExistingFile(fullPath)));
    } else if (file.endsWith('.uncompressed_size')) {
      tasks.push(deleteExistingFile(fullPath));
    }
  }

  const concurrencyLimit = 25;
  for (let i = 0; i < tasks.length; i += concurrencyLimit) {
    await Promise.all(tasks.slice(i, i + concurrencyLimit));
  }
}

async function deleteExistingFile(filePath) {
  try {
    await unlink(filePath);
  } catch (error) {
    if (error.code !== 'ENOENT') {
      throw error;
    }
  }
}

async function decompressChunks(reader, compressionIndex, sizeUnpackedChunk) {
  const dataChunks = [];
  let readData = 0;

  for (const {compressed, uncompressed} of compressionIndex) {
    const compressedData = reader.readChunk(Number(compressed));
    const uncompressedData = await unzip(compressedData);

    if (uncompressedData.length != uncompressed) {
      throw new Error(`Chunk size mismatch`);
    }

    readData += 1;

    if (uncompressedData.length != sizeUnpackedChunk &&
        readData != compressionIndex.length) {
      throw new Error(`Partial Chunks`);
    }

    dataChunks.push(uncompressedData);
  }

  return Buffer.concat(dataChunks);
}

async function unpack(src, dst) {
  try {
    const buffer = fs.readFileSync(src);
    const reader = new UnpackReader(buffer);
    const header = readHeader(reader);
    const compressionIndex = readCompressionIndex(reader, header.sizeUnpacked);
    const data = await decompressChunks(
        reader, compressionIndex, header.sizeUnpackedChunk);
    fs.writeFileSync(dst, data);
    return true;
  } catch (error) {
    throw error;
  }
}

function readHeader(reader) {
  const sigverExpected = 2653586369;
  const sigver = reader.readBigInt64LE();
  const sizeUnpackedChunk = reader.readBigInt64LE();
  const sizePacked = reader.readBigInt64LE();
  const sizeUnpacked = reader.readBigInt64LE();

  if (sigver != sigverExpected) {
    throw new Error(`Incorrect signature`);
  }

  return {sizeUnpackedChunk, sizePacked, sizeUnpacked};
}

function readCompressionIndex(reader, sizeUnpacked) {
  const compressionIndex = [];
  let sizeIndexed = 0;

  while (sizeIndexed < sizeUnpacked) {
    const compressed = reader.readBigInt64LE();
    const uncompressed = reader.readBigInt64LE();
    compressionIndex.push({compressed, uncompressed});
    sizeIndexed += Number(uncompressed);
  }

  if (sizeUnpacked != sizeIndexed) {
    throw new Error(`Header-Index mismatch`);
  }

  return compressionIndex;
}

function downloadMods(modids) {
  // Filter out empty strings and whitespace-only strings
  const validModIds = Array.isArray(modids) ? 
    modids.filter(id => id && id.trim() !== '') : [];
  
  if (validModIds.length === 0) {
    console.log('No valid mod IDs provided. Skipping download.');
    return;
  }

  console.log(`Downloading ${validModIds.length} mods: ${validModIds.join(', ')}`);
  const steamCmdPath = `${
      process.cwd()}\\DumpResources\\server\\Engine\\Binaries\\ThirdParty\\SteamCMD\\Win64\\steamcmd.exe`;
  
  // Check if SteamCMD exists
  if (!fs.existsSync(steamCmdPath)) {
    console.error(`SteamCMD not found at: ${steamCmdPath}`);
    return;
  }
  
  const cmd =
      `${steamCmdPath} +login anonymous +workshop_download_item 834910 ${
          validModIds.join(' +workshop_download_item 834910 ')} +quit`;

  try {
    const output = execSync(cmd, {stdio: 'pipe'}).toString();
    console.log('SteamCMD output:', output);
    
    const failedDownloads = (output.match(/Download item (\d+) failed/g) ||
                           []).map(item => item.match(/\d+/)[0]);

    if (failedDownloads.length > 0) {
      console.error('Retrying failed downloads:', failedDownloads);
      // Prevent infinite recursion by checking if we're making progress
      if (failedDownloads.length < validModIds.length) {
        downloadMods(failedDownloads);
      } else {
        console.error('All downloads failed. Please check mod IDs and SteamCMD setup.');
      }
    }
  } catch (error) {
    console.error('Error downloading mods:', error);
    
    // Prevent infinite recursion
    if (validModIds.length > 1) {
      console.log('Attempting to download mods individually...');
      // Download one by one instead of retrying the whole batch
      validModIds.forEach(id => downloadMods([id]));
    } else {
      console.error(`Failed to download mod ${validModIds[0]}. Check if the mod ID is valid.`);
    }
  }
}


function unpackModMeta(buffer) {
  const reader = new UnpackReader(buffer);
  const modMeta = {};

  const numEntries = reader.readInt32LE();
  for (let i = 0; i < numEntries; i++) {
    const keySize = reader.readInt32LE();
    const key = reader.readChunk(keySize).toString('utf8').replace(/\0/g, '');
    const valueSize = reader.readInt32LE();
    const value =
        reader.readChunk(valueSize).toString('utf8').replace(/\0/g, '');
    modMeta[key] = value;
  }

  return modMeta;
}

function getModInfo(modid) {
  const modPath = path.join(modFolder, modid);
  const modInfoPath = path.join(modPath, 'mod.info');
  if (!fs.existsSync(modInfoPath)) {
    console.log(`No mod.info found for mod ${modid}`);
    return;
  }
  const buffer = fs.readFileSync(modInfoPath);
  const reader = new UnpackReader(buffer);
  const nameSize = reader.readInt32LE();
  const name = reader.readChunk(nameSize).toString('utf8');
  console.log(`\t${name}`);

  const startOffset = reader.getOffset();
  const additionalSize = reader.readInt32LE();

  if (additionalSize > 0) {
    return buffer.subarray(startOffset, buffer.length - 8);
  }

  return Buffer.alloc(0);
}


function createModFile(modid) {
  const modPath = path.join(modFolder, modid);
  const metaPath = path.join(modPath, 'modmeta.info');
  const modFile = path.join(modFolder, `${modid}.mod`);

  if (!fs.existsSync(metaPath)) {
    console.log(`No modmeta.info found for mod ${modid}`);
    return;
  }

  const metaContent = fs.readFileSync(metaPath);
  const modMeta = unpackModMeta(metaContent);
  const localDir = `../../../ShooterGame/Content/Mods/${modid}`;

  const pack64 = Buffer.alloc(8);
  pack64.writeBigUInt64LE(BigInt(modid));

  const infoContent = getModInfo(modid);

  const modContent = Buffer.concat([
    pack64, Buffer.from('00000000', 'hex'),  // 4 bytes of 0
    Buffer.from(
        (localDir.length + 1)
            .toString(16)
            .padStart(8, '0')
            .match(/../g)
            .reverse()
            .join(''),
        'hex'),
    Buffer.from(localDir + '\0', 'utf8'),
    infoContent.length > 0 ? infoContent : Buffer.from('00000000', 'hex'),
    Buffer.from('33FF22FF02000000', 'hex'),
    Buffer.from('0' + modMeta['ModType'], 'hex'), metaContent
  ]);

  try {
    fs.writeFileSync(modFile, modContent);
  } catch (error) {
    console.error(`Failed to create .mod file for ${modid}:`, error);
  }
}

async function installMods(modids) {
  if (modids.length === 0) return;

  const steamFolder =
      'DumpResources/server/Engine/Binaries/ThirdParty/SteamCMD/Win64/steamapps/workshop/content/834910/';

  fs.mkdirSync(modFolder, {recursive: true});

  for (const modid of modids) {
    const source = path.join(steamFolder, modid, 'WindowsNoEditor');
    const dest = path.join(modFolder, modid);

    if (!fs.existsSync(source)) {
      console.error(`Mod ${modid} not found`);
      continue;
    }

    console.log(`Installing mod ${modid}...`);
    try {
      if (fs.existsSync(dest)) {
        fs.rmSync(dest, {recursive: true});
      }
      fs.mkdirSync(dest, {recursive: true});
      fs.cpSync(source, dest, {recursive: true});
    } catch (error) {
      console.error(`Failed to install mod ${modid}:`, error);
    }
    createModFile(modid);
    await unpackZFilesRecursive(dest);
  }
}

module.exports = {
  unpack,
  UnpackReader,
  installMods,
  downloadMods
};
