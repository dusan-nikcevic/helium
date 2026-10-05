const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const Ajv = require('ajv/dist/2020').default;

const read = name => JSON.parse(fs.readFileSync(path.join(__dirname, name), 'utf8'));
const ajv = new Ajv({strict: true, strictTypes: false, strictRequired: false, allErrors: true});
ajv.addSchema(read('command.schema.json'));
const request = ajv.getSchema('urn:zephyr:engine-command:1');
const result = ajv.compile(read('result.schema.json'));
const examples = read('examples.json');
let checks = 0;
for (const [validate, inputs] of [[request, examples.requests], [result, examples.results]]) {
  for (const input of inputs) {
    assert.equal(validate(input), true, JSON.stringify(validate.errors));
    checks++;
  }
}

const rejectRequest = change => {
  const value = structuredClone(examples.requests[0]);
  change(value);
  assert.equal(request(value), false, JSON.stringify(value));
  checks++;
};
for (const change of [
  value => value.schemaVersion = 2,
  value => value.source = 'script',
  value => value.groupMode = 'linked',
  value => value.commandId = '',
  value => value.sessionId = 'selected session',
  value => value.expectedRevision = -1,
  value => value.expectedRevision = 1.5,
  value => value.operations = [],
  value => value.transactionId = 'transaction-1',
  value => value.phase = 'undo',
  value => value.operations[0].value = '6',
  value => value.operations[0].value = 13,
  value => value.operations[0].value = NaN,
  value => value.operations[0].value = Infinity,
  value => value.operations[0].value = -121,
  value => value.operations[0].expected = true,
  value => value.operations[0].control = 'unknown',
  value => value.operations[0].target.kind = 'region',
  value => value.operations[0].target.routeId = 'route-2',
  value => value.operations[0].script = 'arbitrary code',
  value => value.extra = 1,
  value => delete value.selection,
  value => value.operations[0].parameterId = 'threshold',
  value => value.selection = [{kind: 'processor', id: 'processor-1'}],
  value => value.selection = [{kind: 'region', id: 'region-1', routeId: 'route-1'}],
  value => value.selection = [{kind: 'send', id: 'send-1', routeId: 'route-1', playlistId: 'playlist-1'}]
]) rejectRequest(change);

const cases = [
  [3, value => delete value.operations[0].unit],
  [3, value => value.operations[0].value = false],
  [6, value => value.operations[0].value.domain = 'bars'],
  [7, value => value.operations[0].value.value = 1.5],
  [9, value => value.operations = examples.requests[0].operations],
  [9, value => delete value.transactionId]
];
for (const [index, change] of cases) {
  const value = structuredClone(examples.requests[index]);
  change(value);
  assert.equal(request(value), false);
  checks++;
}
for (const [index, change] of [
  [0, value => value.status = 'applied'],
  [0, value => value.stateValid = false],
  [1, value => delete value.transactionId],
  [1, value => value.error = examples.results[3].error],
  [2, value => value.phase = 'apply'],
  [3, value => value.changes = examples.results[0].changes],
  [3, value => delete value.error.code],
  [3, value => value.error.path = '/bad~2escape'],
  [3, value => value.error.path = 'operations/0'],
  [3, value => value.stateValid = false],
  [4, value => value.status = 'applied']
]) {
  const value = structuredClone(examples.results[index]);
  change(value);
  assert.equal(result(value), false);
  checks++;
}
console.log(`Engine contract: ${checks} checks, 0 failures`);
