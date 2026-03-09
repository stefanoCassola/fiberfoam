import type { IncomingMessage, ServerResponse } from 'http'

const GITHUB_REPO = 'stefanoCassola/fiberfoam'

export default async function handler(req: IncomingMessage, res: ServerResponse) {
  // CORS
  res.setHeader('Access-Control-Allow-Origin', '*')
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS')
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type')
  if (req.method === 'OPTIONS') { res.statusCode = 200; res.end(); return }
  if (req.method !== 'POST') { res.statusCode = 405; res.end(JSON.stringify({ error: 'Method not allowed' })); return }

  const token = process.env.GITHUB_TOKEN
  if (!token) { res.statusCode = 500; res.end(JSON.stringify({ error: 'GITHUB_TOKEN not configured' })); return }

  // Parse body
  const chunks: Buffer[] = []
  for await (const chunk of req) chunks.push(typeof chunk === 'string' ? Buffer.from(chunk) : chunk)
  let body: any
  try { body = JSON.parse(Buffer.concat(chunks).toString()) } catch { body = {} }

  const { category = 'General', message, contact = '' } = body
  if (!message || typeof message !== 'string' || !message.trim()) {
    res.statusCode = 400; res.end(JSON.stringify({ error: 'Message is required' })); return
  }

  const labelMap: Record<string, string> = {
    'Bug Report': 'bug',
    'Feature Request': 'enhancement',
    'Question': 'question',
    'General': 'feedback',
  }
  const label = labelMap[category] || 'feedback'

  const issueBody = [
    `**Category:** ${category}`,
    '',
    `**Message:**`,
    message.trim(),
    '',
    contact ? `**Contact:** ${contact}` : '',
    '',
    `---`,
    `*Submitted via FiberFoam feedback form*`,
  ].filter(Boolean).join('\n')

  try {
    const ghRes = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/issues`, {
      method: 'POST',
      headers: {
        Authorization: `token ${token}`,
        Accept: 'application/vnd.github.v3+json',
        'Content-Type': 'application/json',
        'User-Agent': 'FiberFoam-Feedback',
      },
      body: JSON.stringify({
        title: `[Feedback] ${category}: ${message.trim().slice(0, 80)}`,
        body: issueBody,
        labels: [label],
      }),
    })

    if (!ghRes.ok) {
      const err = await ghRes.text()
      console.error('GitHub API error:', ghRes.status, err)
      res.statusCode = 502; res.end(JSON.stringify({ error: 'Failed to create GitHub issue', detail: err })); return
    }

    const issue = await ghRes.json() as any
    res.statusCode = 200
    res.setHeader('Content-Type', 'application/json')
    res.end(JSON.stringify({ status: 'ok', issueUrl: issue.html_url }))
  } catch (err) {
    console.error('Error creating issue:', err)
    res.statusCode = 500; res.end(JSON.stringify({ error: 'Internal error' })); return
  }
}
