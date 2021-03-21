import { logLevel, logFolder, setLogFolder } from "../../src/settings";
import { ParquetToolsRunner } from "../../src/parquet-tools-runner";
import { getUri } from "./utils";
import { promises } from "fs";
import { expect } from "chai";
import path = require("path");
import toArray from '@async-generators/to-array';

suite('logger tests', function () {
  const folder = path.resolve(__dirname);
  const logPath = path.join(folder, 'parquet-viewer.log');

  suiteTeardown(async function() {
    await promises.unlink(logPath);
    await setLogFolder(undefined);
  });

  test('log level', function () {
    expect(logLevel()).to.be.equal('debug');
  });

  test('log file', async function () {
    await setLogFolder(folder);
    expect(logFolder()).to.equal(folder);

    const parquet = await getUri('small.parquet');
    const contents = await toArray(ParquetToolsRunner.toJson(parquet.fsPath));
    const logContents = await promises.readFile(logPath, 'utf-8');

    expect(contents).to.have.lengthOf(2, `contents are ${contents}`);
    expect(logContents).to.match(/\{\s+"label": "parquet-viewer",\s+"level": "debug",\s+"message": "spawning java -jar .*\/workspace\/parquet-tools-1.12.0-SNAPSHOT.jar cat -j .*\/workspace\/small.parquet",\s+"time": "\S+"\s+\}/);
  });
});
