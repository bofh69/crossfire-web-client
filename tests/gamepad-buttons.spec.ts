import { describe, expect, test } from "vitest";
import {
  buttonIdForIndex,
  buttonIndex,
  buttonLabel,
  normaliseButtonId,
} from "../src/lib/gamepad_buttons";

describe("gamepad button ids", () => {
  test("standard names and raw ids resolve to indices", () => {
    expect(buttonIndex("a")).toBe(0);
    expect(buttonIndex("dpright")).toBe(15);
    expect(buttonIndex("guide")).toBe(16);
    expect(buttonIndex("b17")).toBe(17);
    expect(buttonIndex("nonsense")).toBe(-1);
  });

  test("indices get standard names only on standard gamepads", () => {
    expect(buttonIdForIndex(4, true)).toBe("leftshoulder");
    expect(buttonIdForIndex(17, true)).toBe("b17");
    expect(buttonIdForIndex(4, false)).toBe("b4");
  });

  test("old index bindings and names are normalised", () => {
    expect(normaliseButtonId(11, true)).toBe("rightstick");
    expect(normaliseButtonId("b11", true)).toBe("rightstick");
    expect(normaliseButtonId("rightstick", false)).toBe("b11");
    expect(normaliseButtonId(-1, true)).toBeNull();
    expect(normaliseButtonId("bogus", true)).toBeNull();
  });

  test("labels", () => {
    expect(buttonLabel("a")).toBe("A");
    expect(buttonLabel("dpup")).toBe("D-pad up");
    expect(buttonLabel("b17")).toBe("B17");
  });
});
