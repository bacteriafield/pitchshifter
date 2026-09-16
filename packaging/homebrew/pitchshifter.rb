class Pitchshifter < Formula
  desc "Virtual vocal effects pedal inspired by the DigiTech Vocal 300"
  homepage "https://github.com/bacteriafield/pitchshifter"
  url "https://github.com/bacteriafield/pitchshifter/releases/download/v@VERSION@/pitchshifter-macos-arm64.tar.gz"
  sha256 "@SHA256_MACOS@"
  license "MIT"

  depends_on arch: :arm64
  depends_on "lua@5.4"
  depends_on :macos
  depends_on "portaudio"

  def install
    prefix.install Dir["*"]
  end

  test do
    assert_predicate bin/"PitchShifter", :executable?
    system Formula["lua@5.4"].opt_bin/"lua", "-e",
           "package.cpath = '#{lib}/pitchshifter/?.so'; assert(#require('pcore').voices() == 12)"
  end
end
