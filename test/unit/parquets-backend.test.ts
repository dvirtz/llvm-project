import {describe, expect, test, jest} from '@jest/globals';
import toArray from '@async-generators/to-array';
import { createReadStream } from 'fs';
import * as path from 'path';
import { createInterface } from 'readline';
import { ParquetsBackend } from '../../src/parquets-backend';
import { jsonSpace } from '../../src/settings';
import { CancellationToken } from 'vscode';


const rootDir = path.join(__dirname, '..', '..');

jest.mock('vscode', () => {
  const getConfigurationMock = jest.fn();
  getConfigurationMock.mockReturnValue({
    get: jest.fn()
  });
  return {
    workspace: {
      getConfiguration: getConfigurationMock,
    }
  };
});

jest.mock('../../src/settings');

describe("ParquetsBackend tests", () => {
  const backend = new ParquetsBackend();
  const workspace = path.join(rootDir, 'test', 'workspace');

  test.each(
    ["small", "large"]
  )('Converts %s parquet to JSON', async function (name: string) {
    const json = (await toArray(backend.toJson(path.join(workspace, `${name}.parquet`)))).map(line => line.trim());
    const expected = await toArray(createInterface({ input: createReadStream(path.join(workspace, `${name}.json`)) }));

    expect(json).toEqual(expected);
  });

  test("Error on not existing file", async function () {
    await expect(toArray(backend.toJson("no-such-file"))).rejects.toThrow(/ENOENT: no such file or directory, stat '.*no-such-file'/);
  });

  test.each([0, 2, 10, "\t", "###"])('Test space %s', async function (space: number | string) {
    jest.mocked(jsonSpace).mockReturnValue(space);

    const json = (await toArray(backend.toJson(path.join(workspace, `small.parquet`)))).map(line => line.trim());
    const records = await toArray(createInterface({ input: createReadStream(path.join(workspace, `small.json`)) }));
    const expected = records.map(record => JSON.stringify(JSON.parse(record), null, space));

    expect(json).toEqual(expected);
  });

  test("cancellation", async function () {
    const token = {
      get isCancellationRequested() {
        return this.isCancellationRequestedMock();
      },
      isCancellationRequestedMock: jest.fn().mockReturnValueOnce(false).mockReturnValue(true),
      onCancellationRequested: jest.fn()
    };
    expect(await toArray(backend.toJson(path.join(workspace, `small.parquet`), token as CancellationToken))).toHaveLength(1);
    expect(token.isCancellationRequestedMock).toBeCalledTimes(2);
  });
});
