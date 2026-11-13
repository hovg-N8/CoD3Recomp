import test from 'node:test';
import assert from 'node:assert/strict';
import worker from '../hosting/worker.mjs';
const key = 'objects/' + 'a'.repeat(64) + '.gz';
function environment(values = {}) {
  const calls = [];
  return { calls, RELEASES: {
    async get(name) { calls.push(name); return values[name] ? object(values[name]) : null; },
    async head(name) { calls.push(name); return values[name] ? object(values[name]) : null; }
  } };
}
function object(value) { return { body: value, size: Buffer.byteLength(value), httpEtag: '"fixture"' }; }
function request(path, method = 'GET') { return new Request('https://updates.invalid' + path, { method }); }
test('health distinguishes configured service from published content', async () => {
  const response = await worker.fetch(request('/health'), environment());
  assert.equal(response.status, 200); assert.deepEqual(await response.json(), { ok: true, published: false });
});
test('missing release returns explicit 404 without caching', async () => {
  const response = await worker.fetch(request('/stable.json'), environment());
  assert.equal(response.status, 404); assert.equal(response.headers.get('cache-control'), 'no-store');
});
test('manifest is served as JSON with no stale cache', async () => {
  const response = await worker.fetch(request('/stable.json'), environment({ 'stable.json': '{"signed":true}' }));
  assert.equal(response.status, 200); assert.equal(response.headers.get('cache-control'), 'no-store'); assert.equal(await response.text(), '{"signed":true}');
});
test('immutable chunks are streamed with exact length', async () => {
  const env = environment({ [key]: 'packed' }); const response = await worker.fetch(request('/' + key), env);
  assert.equal(response.status, 200); assert.equal(await response.text(), 'packed'); assert.equal(response.headers.get('content-length'), '6'); assert.match(response.headers.get('cache-control'), /immutable/); assert.deepEqual(env.calls, [key]);
});
test('public writes and bucket listing are rejected', async () => {
  const env = environment();
  for (const method of ['POST', 'PUT', 'DELETE', 'PATCH']) assert.equal((await worker.fetch(request('/stable.json', method), env)).status, 405);
  for (const path of ['/', '/private-key.xml', '/objects/', '/candidate.json', '/anything.exe']) assert.equal((await worker.fetch(request(path), env)).status, 404);
  assert.equal(env.calls.length, 0);
});
test('HEAD omits content body', async () => {
  const response = await worker.fetch(request('/' + key, 'HEAD'), environment({ [key]: 'packed' })); assert.equal(response.status, 200); assert.equal(await response.text(), '');
});
test('storage errors become retryable service responses', async () => {
  const response = await worker.fetch(request('/stable.json'), { RELEASES: { async get() { throw new Error('private diagnostic'); } } });
  assert.equal(response.status, 503); assert.doesNotMatch(await response.text(), /private diagnostic/);
});
