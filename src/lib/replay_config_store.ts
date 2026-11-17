/**
 * Imported first by the replay page so that every module loads and saves its
 * configuration in memory instead of the normal client's `localStorage`.
 */
import { useInMemoryConfigStore } from "./storage";

useInMemoryConfigStore();
