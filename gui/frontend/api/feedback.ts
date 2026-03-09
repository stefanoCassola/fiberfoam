import type { VercelRequest, VercelResponse } from '@vercel/node'

const GITHUB_REPO = 'stefanoCassola/fiberfoam'

export default async function handler(req: VercelRequest, res: VercelResponse) {
  // CORS
  res.setHeader('Access-Control-Allow-Origin', '*')
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS')
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type')
  if (req.method === 'OPTIONS') return res.status(200).end()
  if (req.method !== 'POST') return res.status(405).json({ error: 'Method not allowed' })

  const token = process.env.GITHUB_TOKEN
  if (!token) return res.status(500).json({ error: 'GITHUB_TOKEN not configured' })

  const { category = 'General', message, contact = '' } = req.body || {}
  if (!message || typeof message !== 'string' || !message.trim()) {
    return res.status(400).json({ error: 'Message is required' })
  }

  // Map category to GitHub label
  const labelMap: Record<string, string> = {
    'Bug Report': 'bug',
    'Feature Request': 'enhancement',
    'Question': 'question',
    'General': 'feedback',
  }
  const label = labelMap[category] || 'feedback'

  // Build issue body
  const body = [
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
      },
      body: JSON.stringify({
        title: `[Feedback] ${category}: ${message.trim().slice(0, 80)}`,
        body,
        labels: [label],
      }),
    })

    if (!ghRes.ok) {
      const err = await ghRes.text()
      console.error('GitHub API error:', err)
      return res.status(502).json({ error: 'Failed to create GitHub issue' })
    }

    const issue = await ghRes.json()
    return res.status(200).json({ status: 'ok', issueUrl: issue.html_url })
  } catch (err) {
    console.error('Error creating issue:', err)
    return res.status(500).json({ error: 'Internal error' })
  }
}
