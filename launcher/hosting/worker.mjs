export default {
  async fetch(request, env) {
    const respond = (message, status, extra = {}) => new Response(request.method === 'HEAD' ? null : message, {
      status, headers: { 'cache-control': 'no-store', 'x-content-type-options': 'nosniff', ...extra }
    });
    if (!['GET', 'HEAD'].includes(request.method)) return respond('Read-only service', 405, { allow: 'GET, HEAD' });
    const path = new URL(request.url).pathname;
    const chunk = /^\/objects\/[a-f0-9]{64}\.gz$/.test(path);
    if (path !== '/health' && path !== '/stable.json' && !chunk) return respond('Not found', 404);
    try {
      if (path === '/health') {
        const published = Boolean(await env.RELEASES.head('stable.json'));
        return respond(JSON.stringify({ ok: true, published }), 200, { 'content-type': 'application/json' });
      }
      const key = path.slice(1);
      const object = await env.RELEASES[request.method === 'HEAD' ? 'head' : 'get'](key);
      if (!object) return respond(path === '/stable.json' ? 'No release published yet' : 'Content not found', 404);
      return new Response(request.method === 'HEAD' ? null : object.body, { status: 200, headers: {
        'content-type': chunk ? 'application/octet-stream' : 'application/json',
        'content-length': String(object.size),
        'cache-control': chunk ? 'public, max-age=31536000, immutable' : 'no-store',
        'x-content-type-options': 'nosniff',
        etag: object.httpEtag
      } });
    } catch {
      return respond('Update storage is temporarily unavailable. Please retry.', 503, { 'retry-after': '30' });
    }
  }
};
