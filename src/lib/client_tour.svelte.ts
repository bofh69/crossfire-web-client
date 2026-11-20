/**
 * client_tour.svelte.ts — The guided tour of the game screen: help balloons
 * that point at each part of the client, one at a time.
 *
 * The tour starts by itself the first time a player enters the game after
 * creating an account, and can be started again from the Info menu.
 */

import { loadConfig, saveConfig } from "./storage";

/** Config key that is true while a tour waits for the next game login. */
const TOUR_PENDING_KEY = "clientTourPending";

/** One help balloon of the tour. */
export interface ClientTourStep {
  /** Heading of the balloon. */
  title: string;
  /** Paragraphs of text in the balloon. */
  text: string[];
  /**
   * CSS selector of the element the balloon points at, or undefined for a
   * balloon in the middle of the screen.  A step whose element isn't shown
   * is skipped.
   */
  target?: string;
}

/** The balloons of the tour, in the order they are shown. */
export const CLIENT_TOUR_STEPS: readonly ClientTourStep[] = [
  {
    title: "Welcome to Crossfire!",
    text: [
      "This short tour shows the parts of the game screen.",
      "Use Next and Back, or the arrow keys. Escape ends the tour. You can see it again from Info → Client Tour.",
    ],
  },
  {
    title: "The world",
    target: ".map-area",
    text: [
      "You are in the middle of the map. Walk with the arrow keys, Home/End/PageUp/PageDown for the diagonals, or the numpad. Hold Alt to run and Shift to fire or attack in a direction.",
      "Left-click a square to look at it, right-click to walk there.",
    ],
  },
  {
    title: "Your character",
    target: ".stats-section",
    text: [
      "Health, mana, grace and food. Eat before your food runs out, and rest when your health is low.",
      "Below them are your stats, like strength and dexterity.",
    ],
  },
  {
    title: "Items, spells and skills",
    target: ".tab-bar",
    text: [
      "Switch between your inventory, spells, skills, protections, quests and knowledge.",
    ],
  },
  {
    title: "Using things",
    target: ".tab-content",
    text: [
      "Left-click an item or skill to use it, or a spell to ready it. Right-click for more choices, like dropping or binding to the hotbar.",
      "The items on the floor under you are listed here too.",
    ],
  },
  {
    title: "The hotbar",
    target: ".hotbar-area",
    text: [
      "Put items, spells and skills you use often here. F1–F12 use the slots.",
    ],
  },
  {
    title: "Messages and commands",
    target: ".info-area",
    text: [
      "What happens in the game is written here. Press ' (apostrophe) to type a command, \" to say something, or Enter to chat.",
      "Talk to people by saying hello next to them, and click the replies they give.",
    ],
  },
  {
    title: "Menus",
    target: ".menu-area",
    text: [
      "Change settings like the music and sound volume, key bindings and gamepad buttons. Info → Help explains more of the game.",
      "Press Tab to move around the screen with the keyboard or a gamepad.",
    ],
  },
  {
    title: "Make it your own",
    target: ".resize-handle-col",
    text: [
      "Drag the borders between the map and the panels to make them bigger or smaller.",
    ],
  },
  {
    title: "Good luck!",
    text: [
      "Explore the town, talk to people, and find someone who needs your help. Type 'help' for the game's own help.",
    ],
  },
];

/** Whether the tour is running, and its current step. */
class ClientTour {
  /** True while the tour is shown. */
  active = $state(false);

  /**
   * Show the tour the next time a character enters the game, even after a
   * reload.  Called when the player creates a new account.
   */
  requestOnNextLogin(): void {
    saveConfig(TOUR_PENDING_KEY, true);
  }

  /** Forget a tour requested by {@link requestOnNextLogin}. */
  cancelRequest(): void {
    saveConfig(TOUR_PENDING_KEY, false);
  }

  /** Start the tour if one was requested, and forget the request. */
  startIfRequested(): void {
    if (!loadConfig(TOUR_PENDING_KEY, false)) return;
    this.cancelRequest();
    this.start();
  }

  /** Start the tour from the first step. */
  start(): void {
    this.active = true;
  }

  /** End the tour. */
  stop(): void {
    this.active = false;
  }
}

/** The client's tour. */
export const clientTour = new ClientTour();
