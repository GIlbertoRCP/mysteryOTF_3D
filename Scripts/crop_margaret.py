from PIL import Image

im = Image.open(r"B:\mysteryOTF\Saved\Screenshots\WindowsEditor\ScreenShot00263.png")
# Screen is 2560 x 1392. Margaret is in the center-left.
# Let's crop around center (x: 800 to 1400, y: 500 to 1200)
crop = im.crop((800, 500, 1400, 1200))
crop.save(r"C:\Users\huevi\.gemini\antigravity-ide\brain\8f745b1b-9d5e-4ed4-9b0d-e0aa509814fc\scratch\margaret_crop.png")
print("Saved margaret_crop.png")
