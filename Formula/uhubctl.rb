class Uhubctl < Formula
  desc "USB hub per-port power control"
  homepage "https://github.com/mvp/uhubctl"
  head "https://github.com/mvp/uhubctl.git"
  url "https://github.com/mvp/uhubctl/archive/v2.6.0.tar.gz"
  sha256 "56ca15ddf96d39ab0bf8ee12d3daca13cea45af01bcd5a9732ffcc01664fdfa2"
  license "GPL-2.0"

  depends_on "libusb"
  depends_on "pkg-config" => :build

  livecheck do
    url :stable
  end

  def install
    system "make"
    bin.install "uhubctl"
  end

  test do
    system "#{bin}/uhubctl", "-v"
  end
end
