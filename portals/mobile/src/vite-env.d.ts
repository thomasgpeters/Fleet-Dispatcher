/// <reference types="vite/client" />

interface ImportMetaEnv {
  /** Base URL of the ApiLogicServer JSON:API (e.g. http://localhost:5659/api). */
  readonly VITE_API_BASE_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
