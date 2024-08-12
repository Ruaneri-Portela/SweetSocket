public class WebMain {
    public boolean requestSuccessful = false;
    public byte[] body;
    public String contentType;
    public String headerAppend;
    public int statusCode;

    public WebMain(String header, byte[] reciveBody, int size) {
        String path = header.split(" ")[1];
        String prefix = "/java";

        if (!path.startsWith(prefix))
            return;
    
        String html = "<html><body><h1>Hello, World From JAVA!</h1><div>%s</div></body></html>";
        if (size > 0) {
            String paragraph = "<p>Body: " + new String(reciveBody) + "</p>";
            html = String.format(html, paragraph);
        } else {
            html = String.format(html, "");
        }
        
        body = html.getBytes();
        contentType = "text/html";
        statusCode = 200;
        requestSuccessful = true;
        return;
    }
}
