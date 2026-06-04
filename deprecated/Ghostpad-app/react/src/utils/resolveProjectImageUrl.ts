/** Resolve project thumbnails so they work on nested routes like /projects/:id */
export function resolveProjectImageUrl(imageUrl?: string): string | undefined {
  if (!imageUrl || !imageUrl.trim()) return undefined;
  if (
    imageUrl.startsWith("data:") ||
    imageUrl.startsWith("http://") ||
    imageUrl.startsWith("https://") ||
    imageUrl.startsWith("blob:")
  ) {
    return imageUrl;
  }
  const path = imageUrl.replace(/^\.\//, "").replace(/^\/+/, "");
  const base = (process.env.PUBLIC_URL || "").replace(/\/$/, "");
  return `${base}/${path}`;
}
